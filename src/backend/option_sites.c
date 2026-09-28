// SPDX-License-Identifier: GPL-3.0-or-later

#include "backend/option_sites.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Loaded once before any worker starts and read-only while chunks are
// emitted, so the lookups need no locking.
static OptionSite* g_sites = NULL;
static u32 g_count = 0;

static bool parse_u32(const char* text, u32* out) {
    char* end = NULL;
    unsigned long value = strtoul(text, &end, 0);
    if (end == text || *end != '\0' || value > 0xFFFFFFFFul)
        return false;
    *out = (u32)value;
    return true;
}

bool option_sites_load(const char* path) {
    FILE* file = fopen(path, "r");
    if (!file) {
        fprintf(stderr, "error: can't open option sites '%s'\n", path);
        return false;
    }
    char line[512];
    u32 number = 0;
    bool ok = true;
    while (ok && fgets(line, sizeof line, file)) {
        ++number;
        char* comment = strchr(line, '#');
        if (comment)
            *comment = '\0';
        char where[128], option[32], kind[16], value[32];
        const int fields = sscanf(line, "%127s %31s %15s %31s", where, option, kind, value);
        if (fields <= 0)
            continue;
        OptionSite site;
        memset(&site, 0, sizeof site);
        ok = fields == 4 && parse_u32(option, &site.option) && site.option < 256u &&
             parse_u32(value, &site.value);
        if (ok) {
            if (strcmp(kind, "insn") == 0)
                site.kind = OPTION_SITE_INSN;
            else if (strcmp(kind, "imm16") == 0 && site.value <= 0xFFFFu)
                site.kind = OPTION_SITE_IMM16;
            else if (strcmp(kind, "hook") == 0)
                site.kind = OPTION_SITE_HOOK;
            else if (strcmp(kind, "after") == 0)
                site.kind = OPTION_SITE_HOOK_AFTER;
            else
                ok = false;
        }
        if (ok) {
            char* plus = strchr(where, '+');
            if (plus) {
                *plus = '\0';
                ok = strlen(where) < sizeof site.rel && parse_u32(plus + 1, &site.offset);
                if (ok)
                    strcpy(site.rel, where);
            } else {
                ok = parse_u32(where, &site.offset);
            }
        }
        if (ok) {
            OptionSite* grown = (OptionSite*)realloc(g_sites, (g_count + 1u) * sizeof *grown);
            ok = grown != NULL;
            if (ok) {
                g_sites = grown;
                g_sites[g_count++] = site;
            }
        }
        if (!ok)
            fprintf(stderr, "error: %s:%u: expected `WHERE OPTION insn|imm16|hook|after VALUE`\n",
                    path, number);
    }
    fclose(file);
    if (ok)
        printf("option sites: %u from %s\n", g_count, path);
    return ok;
}

bool option_sites_active(void) { return g_count != 0; }

void option_sites_resolve_dol(void) {
    for (u32 i = 0; i < g_count; ++i) {
        if (g_sites[i].rel[0] == '\0')
            g_sites[i].address = g_sites[i].offset;
    }
}

void option_sites_resolve_rel(const char* rel_path, u32 base) {
    const char* slash = strrchr(rel_path, '/');
    const char* name = slash ? slash + 1 : rel_path;
    for (u32 i = 0; i < g_count; ++i) {
        if (g_sites[i].rel[0] != '\0' && strcmp(g_sites[i].rel, name) == 0)
            g_sites[i].address = base + g_sites[i].offset;
    }
}

const OptionSite* option_site_at(u32 address) {
    for (u32 i = 0; i < g_count; ++i) {
        if (g_sites[i].address != 0u && g_sites[i].address == address)
            return &g_sites[i];
    }
    return NULL;
}

bool option_sites_in_range(u32 start, u32 end) {
    for (u32 i = 0; i < g_count; ++i) {
        if (g_sites[i].address != 0u && g_sites[i].address >= start && g_sites[i].address < end)
            return true;
    }
    return false;
}

bool option_site_alternative(const OptionSite* site, const PPCInst* original, PPCInst* out) {
    u32 raw;
    if (site->kind == OPTION_SITE_INSN)
        raw = site->value;
    else if (site->kind == OPTION_SITE_IMM16)
        raw = (original->raw & 0xFFFF0000u) | site->value;
    else
        return false;
    *out = ppc_decode(raw, original->address);
    return true;
}
