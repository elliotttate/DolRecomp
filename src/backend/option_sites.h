// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DOLRECOMP_OPTION_SITES_H
#define DOLRECOMP_OPTION_SITES_H

#include "../common/types.h"
#include "../frontend/decoder.h"

// Option sites: instructions whose behaviour a runtime switch changes. The C
// backend emits both behaviours at a site and picks one with
// dolrecomp_option_flags[option], which the runtime owns, so a game's
// settings can be turned on and off without translating it again.
//
//   insn   the instruction is replaced by `value` while the switch is on
//   imm16  its low 16 bits (an immediate or displacement) become `value`
//   hook   dolrecomp_native_hook(ctx, value) runs before the instruction
//   after  dolrecomp_native_hook(ctx, value) runs after it (not a branch)
//
// A hook returns 0 to go on, or the guest address to continue at.
typedef enum {
    OPTION_SITE_INSN,
    OPTION_SITE_IMM16,
    OPTION_SITE_HOOK,
    OPTION_SITE_HOOK_AFTER,
} OptionSiteKind;

typedef struct {
    char rel[64];   // module file name, or "" for the DOL
    u32 offset;     // DOL address, or offset in the REL file
    u32 address;    // the translated address; 0 until resolved
    u32 option;     // index into dolrecomp_option_flags
    OptionSiteKind kind;
    u32 value;
} OptionSite;

// One site per line: `WHERE OPTION KIND VALUE`, where WHERE is a DOL address
// or `module.rel+OFFSET`; `#` starts a comment.
bool option_sites_load(const char* path);
bool option_sites_active(void);
// DOL sites translate at their own address; a REL's at its base plus offset.
void option_sites_resolve_dol(void);
void option_sites_resolve_rel(const char* rel_path, u32 base);
const OptionSite* option_site_at(u32 address);
bool option_sites_in_range(u32 start, u32 end);
// The instruction a site substitutes (insn and imm16 sites).
bool option_site_alternative(const OptionSite* site, const PPCInst* original,
                             PPCInst* out);

#endif
