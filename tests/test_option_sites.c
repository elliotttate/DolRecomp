#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/backend/c_cfg.h"
#include "../src/backend/emitter.h"
#include "../src/backend/option_sites.h"
#include "../src/frontend/decoder.h"

#define BASE 0x80004000u

static int failures;

static void check(bool condition, const char* message) {
    if (condition)
        return;
    fprintf(stderr, "FAIL: %s\n", message);
    failures++;
}

static bool write_file(const char* path, const char* text) {
    FILE* file = fopen(path, "w");
    if (!file)
        return false;
    fputs(text, file);
    return fclose(file) == 0;
}

int main(void) {
    char path[] = "/tmp/dolrecomp_option_sites_XXXXXX";
    const int fd = mkstemp(path);
    if (fd < 0)
        return 1;
    fclose(fdopen(fd, "w"));

    check(write_file(path, "0x80004000 1 bogus 0\n") && !option_sites_load(path),
          "reject an unknown kind");
    // A loop (subi, cmpwi, bne back) with a substituted compare, a hook before
    // the return, and a branch the switch makes unconditional into the middle
    // of a straight run.
    check(write_file(path,
                     "# comment\n"
                     "0x80004004 3 imm16 0x0005   # cmpwi r3, 0 -> 5\n"
                     "0x8000400C 4 hook 7\n"
                     "0x80004010 5 insn 0x48000008 # nop -> b +8\n"
                     "module.rel+0x10 6 insn 0x60000000\n") &&
              option_sites_load(path),
          "load sites");
    option_sites_resolve_dol();
    option_sites_resolve_rel("dir/module.rel", 0xC0400000u);
    check(option_site_at(0x80004004u) && option_site_at(0x80004004u)->option == 3u, "find a DOL site");
    check(option_site_at(0xC0400010u) != NULL, "resolve a REL site at its base plus offset");
    check(option_sites_in_range(0x80004000u, 0x80004008u) && !option_sites_in_range(0x80004020u, 0x80004030u),
          "range query");

    static const u32 words[] = {
        0x3863FFFFu, // subi r3, r3, 1
        0x2C030000u, // cmpwi r3, 0
        0x4082FFF8u, // bne 0x80004000
        0x4E800020u, // blr
        0x60000000u, // nop
        0x60000000u, // nop
        0x60000000u, // nop
        0x4E800020u, // blr
    };
    const u32 count = sizeof(words) / sizeof(words[0]);
    PPCInst insts[sizeof(words) / sizeof(words[0])];
    for (u32 i = 0; i < count; ++i)
        insts[i] = ppc_decode(words[i], BASE + i * 4u);
    PPCInst alternative;
    check(option_site_alternative(option_site_at(0x80004004u), &insts[1], &alternative) &&
              alternative.op == insts[1].op && alternative.simm == 5,
          "substitute the low 16 bits");

    CFunctionCFG cfg;
    check(c_function_cfg_build(&cfg, insts, count, BASE), "build the CFG");
    check(cfg.loop_ends[0] == UINT32_MAX, "keep a loop with a site in the flat function");
    check(cfg.leaders[1] && cfg.leaders[2], "a site is a block of its own");
    check(cfg.leaders[6], "a substituted branch lands on a block start");
    c_function_cfg_destroy(&cfg);

    FILE* out = tmpfile();
    check(out && emit_function(out, insts, count, BASE), "emit the function");
    if (out) {
        const long size = ftell(out);
        char* text = (char*)calloc((size_t)size + 1u, 1u);
        rewind(out);
        if (text && fread(text, 1, (size_t)size, out) == (size_t)size) {
            check(strstr(text, "if (dolrecomp_option_flags[3u]) {") != NULL, "switch the substituted compare");
            check(strstr(text, "dolrecomp_native_hook(ctx, 7u)") != NULL, "call the hook");
            check(strstr(text, "goto label_80004018;") != NULL, "take the substituted branch locally");
            check(strstr(text, "loop_80004000") == NULL, "no outlined loop around a site");
        }
        free(text);
        fclose(out);
    }
    remove(path);
    if (failures == 0)
        printf("option_sites: all checks passed\n");
    return failures == 0 ? 0 : 1;
}
