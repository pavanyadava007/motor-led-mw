/* (from github.com/pavanyadava007/motor-temp-edge, MAXR raised to 64)
 * QEMU TCG plugin: count guest instructions executed inside address ranges.
 * Args: g<k>=<name>@<start>-<end>[@<start>-<end>...]   (hex addresses, end exclusive)
 * Calls are counted at the first range's start address (the public entry point).
 * Output is QEMU *emulated instruction count*, not CPU cycles. */
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <qemu-plugin.h>

QEMU_PLUGIN_EXPORT int qemu_plugin_version = QEMU_PLUGIN_VERSION;

#define MAXG 8
#define MAXR 64
typedef struct {
    char name[64];
    int nr;
    uint64_t s[MAXR], e[MAXR];
    uint64_t insns, calls;
} grp_t;
static grp_t g[MAXG];
static int ng;

static void tb_trans(qemu_plugin_id_t id, struct qemu_plugin_tb *tb)
{
    size_t i, n = qemu_plugin_tb_n_insns(tb);
    int k, r;
    (void)id;
    for (i = 0; i < n; i++) {
        struct qemu_plugin_insn *insn = qemu_plugin_tb_get_insn(tb, i);
        uint64_t va = qemu_plugin_insn_vaddr(insn);
        for (k = 0; k < ng; k++) {
            for (r = 0; r < g[k].nr; r++) {
                if (va >= g[k].s[r] && va < g[k].e[r]) {
                    qemu_plugin_register_vcpu_insn_exec_inline(insn, QEMU_PLUGIN_INLINE_ADD_U64, &g[k].insns, 1);
                    if (r == 0 && va == g[k].s[0])
                        qemu_plugin_register_vcpu_insn_exec_inline(insn, QEMU_PLUGIN_INLINE_ADD_U64, &g[k].calls, 1);
                    break;
                }
            }
        }
    }
}

static void at_exit(qemu_plugin_id_t id, void *p)
{
    char buf[256];
    int k;
    (void)id;
    (void)p;
    for (k = 0; k < ng; k++) {
        snprintf(buf, sizeof buf, "INSN_RANGE %s insns=%" PRIu64 " calls=%" PRIu64 "\n", g[k].name, g[k].insns,
                 g[k].calls);
        qemu_plugin_outs(buf);
    }
}

QEMU_PLUGIN_EXPORT int qemu_plugin_install(qemu_plugin_id_t id, const qemu_info_t *info, int argc, char **argv)
{
    int i;
    (void)info;
    for (i = 0; i < argc && ng < MAXG; i++) {
        char *eq = strchr(argv[i], '=');
        char *tok, *save = NULL;
        grp_t *G = &g[ng];
        if (!eq) return -1;
        tok = strtok_r(eq + 1, "@", &save);
        if (!tok) return -1;
        snprintf(G->name, sizeof G->name, "%s", tok);
        while ((tok = strtok_r(NULL, "@", &save)) && G->nr < MAXR) {
            char *dash = strchr(tok, '-');
            if (!dash) return -1;
            G->s[G->nr] = strtoull(tok, NULL, 16);
            G->e[G->nr] = strtoull(dash + 1, NULL, 16);
            G->nr++;
        }
        ng++;
    }
    qemu_plugin_register_vcpu_tb_trans_cb(id, tb_trans);
    qemu_plugin_register_atexit_cb(id, at_exit, NULL);
    return 0;
}
