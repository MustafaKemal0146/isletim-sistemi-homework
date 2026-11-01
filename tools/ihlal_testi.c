/* ihlal_testi.c - prove what happens on a forbidden syscall, in both modes.
 *
 *   ihlal_testi --profil dosya.seccomp [--syscall ad]     (default: unshare)
 *
 * For each of kill/errno a child loads the profile, then calls a syscall that
 * is NOT on the allow list. The parent checks the expected outcome:
 *   kill  -> child dies from SIGSYS
 *   errno -> the call returns -1/EPERM and the child lives (exit 42)
 * Exit 0 only if both match.
 */
#include <errno.h>
#include <seccomp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>

#include "ortak.h"
#include "profil_oku.h"

#define CIKIS_EPERM_ALINDI 42

static void cocuk(const izin_listesi *iz, enum ihlal_modu mod, int nr)
{
    scmp_filter_ctx ctx = filtre_kur(iz, mod_eylemi(mod));
    if (!ctx) _exit(CIKIS_SECCOMP);
    alarm(5); /* safety net if the profile lacks exit_group */
    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0) _exit(CIKIS_SISTEM);
    if (seccomp_load(ctx) != 0) _exit(CIKIS_SECCOMP);
    seccomp_release(ctx);

    long r = syscall(nr, 0, 0, 0);
    int e = errno;
    char msg[96];
    int n = snprintf(msg, sizeof msg, "  [cocuk] syscall dondu: ret=%ld errno=%d (%s)\n", r, r < 0 ? e : 0,
                     r < 0 ? strerror(e) : "ok");
    if (n > 0) (void)!write(1, msg, (size_t)n);
    _exit(r == -1 && e == EPERM ? CIKIS_EPERM_ALINDI : 43);
}

static int dene(const izin_listesi *iz, enum ihlal_modu mod, int nr, const char *ad)
{
    const char *mad = mod == MOD_KILL ? "kill" : "errno";
    printf("[ihlal] mod=%s syscall=%s (nr=%d)\n", mad, ad, nr);
    fflush(stdout);
    pid_t p = fork();
    if (p < 0) { perror("fork"); return 0; }
    if (p == 0) cocuk(iz, mod, nr);
    int st;
    if (waitpid(p, &st, 0) < 0) { perror("waitpid"); return 0; }

    int ok;
    if (WIFSIGNALED(st)) {
        printf("[ihlal]   sonuc: sinyal %d (%s) ile sonlandi\n", WTERMSIG(st), strsignal(WTERMSIG(st)));
        ok = mod == MOD_KILL && WTERMSIG(st) == SIGSYS;
    } else {
        printf("[ihlal]   sonuc: cikis kodu %d\n", WEXITSTATUS(st));
        ok = mod == MOD_ERRNO && WEXITSTATUS(st) == CIKIS_EPERM_ALINDI;
    }
    printf("[ihlal]   beklenen davranis: %s\n", ok ? "EVET" : "HAYIR");
    return ok;
}

int main(int argc, char **argv)
{
    const char *profil = NULL, *ad = "unshare";
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--profil") && i + 1 < argc) profil = argv[++i];
        else if (!strcmp(argv[i], "--syscall") && i + 1 < argc) ad = argv[++i];
        else { fprintf(stderr, "kullanim: ihlal_testi --profil dosya.seccomp [--syscall ad]\n"); return CIKIS_KULLANIM; }
    }
    if (!profil) { fprintf(stderr, "ihlal_testi: --profil gerekli\n"); return CIKIS_KULLANIM; }

    izin_listesi iz = { 0 };
    if (izin_oku(profil, &iz) < 0) return CIKIS_PROFIL;
    int nr = seccomp_syscall_resolve_name(ad);
    if (nr == __NR_SCMP_ERROR) { fprintf(stderr, "ihlal_testi: bilinmeyen syscall '%s'\n", ad); izin_serbest(&iz); return CIKIS_KULLANIM; }
    if (izin_var(&iz, ad)) {
        fprintf(stderr, "ihlal_testi: '%s' profilde IZINLI; ihlal sayilmaz, baska syscall secin\n", ad);
        izin_serbest(&iz);
        return CIKIS_KULLANIM;
    }
    int a = dene(&iz, MOD_KILL, nr, ad);
    int b = dene(&iz, MOD_ERRNO, nr, ad);
    izin_serbest(&iz);
    printf("[ihlal] genel: %s\n", a && b ? "BASARILI" : "BASARISIZ");
    return a && b ? CIKIS_OK : 1;
}
