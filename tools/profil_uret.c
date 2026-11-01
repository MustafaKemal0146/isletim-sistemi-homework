/* profil_uret.c - turn JSON profile(s) into a libseccomp allow-list.
 *
 *   profil_uret girdi.json [girdi2.json...] -o profiller/hedef.seccomp [--bpf f.bpf] [--ozet]
 *
 * Output is deterministic: names are sorted and deduplicated, no timestamps.
 * The .bpf file is the compiled classic-BPF program (seccomp_export_bpf).
 */
#include <errno.h>
#include <fcntl.h>
#include <seccomp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "json_mini.h"
#include "ortak.h"
#include "profil_oku.h"

static int kullanim(void)
{
    fprintf(stderr, "kullanim: profil_uret girdi.json... -o cikti.seccomp [--bpf cikti.bpf] [--ozet]\n");
    return CIKIS_KULLANIM;
}

/* Number of syscalls libseccomp knows for the native arch (the "attack surface"). */
static int toplam_syscall(void)
{
    int n = 0;
    for (int i = 0; i < 1024; i++) {
        char *ad = seccomp_syscall_resolve_num_arch(SCMP_ARCH_NATIVE, i);
        if (ad) { n++; free(ad); }
    }
    return n;
}

int main(int argc, char **argv)
{
    const char *cikti = NULL, *bpf = NULL;
    const char *girdiler[64];
    int ng = 0, ozet = 0;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-o") && i + 1 < argc) cikti = argv[++i];
        else if (!strcmp(argv[i], "--bpf") && i + 1 < argc) bpf = argv[++i];
        else if (!strcmp(argv[i], "--ozet")) ozet = 1;
        else if (argv[i][0] == '-') return kullanim();
        else if (ng < 64) girdiler[ng++] = argv[i];
    }
    if (!cikti || ng == 0) return kullanim();

    sc_liste l = { 0 };
    int rc = CIKIS_OK;
    for (int i = 0; i < ng; i++) {
        size_t n;
        char hata[128];
        char *m = dosya_oku(girdiler[i], &n);
        if (!m) {
            fprintf(stderr, "profil_uret: '%s' okunamadi: %s\n", girdiler[i], strerror(errno));
            rc = CIKIS_PROFIL;
            goto cik;
        }
        if (js_profil_oku(m, n, &l, hata, sizeof hata) < 0) {
            fprintf(stderr, "profil_uret: '%s' bozuk JSON: %s\n", girdiler[i], hata);
            free(m);
            rc = CIKIS_PROFIL;
            goto cik;
        }
        free(m);
    }
    sc_sirala(&l);

    /* Resolve every name first: an unknown name is an error, never silently dropped. */
    izin_listesi iz = { 0 };
    for (size_t i = 0; i < l.n; i++) {
        if (seccomp_syscall_resolve_name(l.k[i].ad) == __NR_SCMP_ERROR) {
            fprintf(stderr, "profil_uret: bilinmeyen syscall '%s' (libseccomp %d.%d.%d)\n", l.k[i].ad,
                    SCMP_VER_MAJOR, SCMP_VER_MINOR, SCMP_VER_MICRO);
            rc = CIKIS_PROFIL;
            goto cik;
        }
        if (iz.n == iz.kap) {
            size_t nk = iz.kap ? iz.kap * 2 : 64;
            char **p = realloc(iz.ad, nk * sizeof *p);
            if (!p) { rc = CIKIS_SISTEM; goto cik2; }
            iz.ad = p;
            iz.kap = nk;
        }
        iz.ad[iz.n] = strdup(l.k[i].ad);
        if (!iz.ad[iz.n]) { rc = CIKIS_SISTEM; goto cik2; }
        iz.n++;
    }

    scmp_filter_ctx ctx = filtre_kur(&iz, SCMP_ACT_KILL_PROCESS);
    if (!ctx) { rc = CIKIS_SECCOMP; goto cik2; }

    FILE *f = fopen(cikti, "w");
    if (!f) {
        fprintf(stderr, "profil_uret: '%s' yazilamadi: %s\n", cikti, strerror(errno));
        rc = CIKIS_SISTEM;
        seccomp_release(ctx);
        goto cik2;
    }
    fprintf(f, "%s\n# arch: x86_64\n# izinli: %zu\n", PROFIL_BASLIK, iz.n);
    for (size_t i = 0; i < iz.n; i++) fprintf(f, "izin %s\n", iz.ad[i]);
    if (fclose(f) != 0) { perror("fclose"); rc = CIKIS_SISTEM; }

    if (bpf) {
        int fd = open(bpf, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0 || seccomp_export_bpf(ctx, fd) < 0) {
            fprintf(stderr, "profil_uret: bpf yazilamadi: %s\n", bpf);
            rc = CIKIS_SECCOMP;
        }
        if (fd >= 0) close(fd);
    }
    seccomp_release(ctx);

    if (rc == CIKIS_OK) {
        int toplam = toplam_syscall();
        int kapali = toplam - (int)iz.n;
        fprintf(stderr, "profil_uret: %zu izinli -> %s\n", iz.n, cikti);
        if (ozet)
            printf("toplam_x86_64=%d izinli=%zu kapatilan=%d oran=%.1f%%\n", toplam, iz.n, kapali,
                   toplam ? 100.0 * kapali / toplam : 0.0);
    }
cik2:
    izin_serbest(&iz);
cik:
    sc_serbest(&l);
    return rc;
}
