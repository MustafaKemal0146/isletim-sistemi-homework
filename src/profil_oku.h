/* profil_oku.h - load a .seccomp rule file and build libseccomp filters.
 *
 * .seccomp format (text, one rule per line, written by tools/profil_uret):
 *   # seccomp-izin-listesi v1
 *   # <comments>
 *   izin <syscall_name>
 */
#ifndef PROFIL_OKU_H
#define PROFIL_OKU_H

#include <errno.h>
#include <seccomp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ortak.h"

#define PROFIL_BASLIK "# seccomp-izin-listesi v1"

typedef struct {
    char **ad;
    size_t n, kap;
} izin_listesi;

static inline void izin_serbest(izin_listesi *l)
{
    for (size_t i = 0; i < l->n; i++) free(l->ad[i]);
    free(l->ad);
    l->ad = NULL;
    l->n = l->kap = 0;
}

static inline int izin_var(const izin_listesi *l, const char *ad)
{
    for (size_t i = 0; i < l->n; i++)
        if (strcmp(l->ad[i], ad) == 0) return 1;
    return 0;
}

/* 0 ok; -1 on error (message printed to stderr). */
static inline int izin_oku(const char *yol, izin_listesi *l)
{
    FILE *f = fopen(yol, "r");
    if (!f) {
        fprintf(stderr, "profil: '%s' acilamadi: %s\n", yol, strerror(errno));
        return -1;
    }
    char satir[256];
    int no = 0;
    while (fgets(satir, sizeof satir, f)) {
        no++;
        size_t len = strlen(satir);
        if (len && satir[len - 1] == '\n') satir[--len] = '\0';
        else if (len == sizeof satir - 1) goto bozuk;
        if (no == 1 && strcmp(satir, PROFIL_BASLIK) != 0) goto bozuk;
        if (!len || satir[0] == '#') continue;
        if (strncmp(satir, "izin ", 5) != 0 || !satir[5]) goto bozuk;
        const char *ad = satir + 5;
        for (const char *c = ad; *c; c++)
            if (!((*c >= 'a' && *c <= 'z') || (*c >= '0' && *c <= '9') || *c == '_')) goto bozuk;
        if (l->n == l->kap) {
            size_t nk = l->kap ? l->kap * 2 : 64;
            char **p = realloc(l->ad, nk * sizeof *p);
            if (!p) goto bozuk;
            l->ad = p;
            l->kap = nk;
        }
        l->ad[l->n] = strdup(ad);
        if (!l->ad[l->n]) goto bozuk;
        l->n++;
    }
    if (no == 0) goto bozuk;
    fclose(f);
    return 0;
bozuk:
    fprintf(stderr, "profil: '%s' bozuk veya taninmayan bicim (satir %d)\n", yol, no);
    fclose(f);
    izin_serbest(l);
    return -1;
}

/* Default action for violations by mode; MOD_YOK has none. */
static inline uint32_t mod_eylemi(enum ihlal_modu m)
{
    switch (m) {
    case MOD_KILL:  return SCMP_ACT_KILL_PROCESS;
    case MOD_ERRNO: return SCMP_ACT_ERRNO(EPERM);
    case MOD_LOG:   return SCMP_ACT_LOG;
    case MOD_SAY:   return SCMP_ACT_TRACE(1);
    default:        return SCMP_ACT_ALLOW;
    }
}

/* Build (not load) a filter: allow every listed syscall, `varsayilan` otherwise. */
static inline scmp_filter_ctx filtre_kur(const izin_listesi *l, uint32_t varsayilan)
{
    scmp_filter_ctx ctx = seccomp_init(varsayilan);
    if (!ctx) return NULL;
    /* Foreign ABI (x32, i386 via int 0x80) must be fatal for the whole process in
     * every mode; libseccomp's default there is the weaker KILL_THREAD. */
    if (seccomp_attr_set(ctx, SCMP_FLTATR_ACT_BADARCH, SCMP_ACT_KILL_PROCESS) < 0) {
        seccomp_release(ctx);
        return NULL;
    }
    for (size_t i = 0; i < l->n; i++) {
        int nr = seccomp_syscall_resolve_name(l->ad[i]);
        if (nr == __NR_SCMP_ERROR) {
            fprintf(stderr, "profil: bilinmeyen syscall '%s'\n", l->ad[i]);
            seccomp_release(ctx);
            return NULL;
        }
        int rc = seccomp_rule_add(ctx, SCMP_ACT_ALLOW, nr, 0);
        if (rc < 0) {
            fprintf(stderr, "profil: kural eklenemedi '%s': %s\n", l->ad[i], strerror(-rc));
            seccomp_release(ctx);
            return NULL;
        }
    }
    return ctx;
}

#endif /* PROFIL_OKU_H */
