/* arch_testi.c - does the filter catch syscalls made through another ABI?
 *
 *   arch_testi --profil dosya.seccomp
 *
 * A filter that only compares `nr` against the allow list could be bypassed by
 * calling an ALLOWED syscall through a different ABI:
 *   x32  : syscall(0x40000000 + nr)   (nr has the x32 bit set)
 *   i386 : int $0x80 with i386 numbers (compat arch, seccomp_data.arch differs)
 * `write` is on the allow list, so these calls must be stopped by the ARCH
 * check, not by the allow list. Variants per mode (kill / errno):
 *   x32, int80, and x32 with a SIGSYS handler installed (a catchable
 *   SECCOMP_RET_KILL_THREAD would run the handler and survive; KILL_PROCESS
 *   cannot be caught).
 * The marker string reaching stdout would mean the call really executed.
 * Foreign-ABI calls are fatal (SIGSYS) in both modes: filtre_kur() sets
 * SCMP_FLTATR_ACT_BADARCH = KILL_PROCESS.
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

#define MARKER "ABI-CAGRISI-CALISTI\n"
#define CIKIS_EPERM 42
#define CIKIS_BASARI_SANILAN 43 /* call succeeded or handler survived: filter failed */
#define CIKIS_BASKA 44

enum yol { YOL_X32, YOL_INT80, YOL_X32_HANDLER };

static void handler(int sig)
{
    (void)sig;
    static const char m[] = "SIGSYS-YAKALANDI-SUREC-YASIYOR\n";
    (void)!write(1, m, sizeof m - 1);
    _exit(CIKIS_BASARI_SANILAN);
}

static long int80_write(const char *m, size_t n)
{
    long r;
    __asm__ volatile("int $0x80" : "=a"(r) : "a"(4L), "b"(1L), "c"(m), "d"(n) : "memory");
    return r;
}

static void cocuk(const izin_listesi *iz, enum ihlal_modu mod, enum yol yol)
{
    if (yol == YOL_X32_HANDLER) {
        struct sigaction sa;
        memset(&sa, 0, sizeof sa);
        sa.sa_handler = handler;
        sigaction(SIGSYS, &sa, NULL);
    }
    scmp_filter_ctx ctx = filtre_kur(iz, mod_eylemi(mod));
    if (!ctx) _exit(CIKIS_SECCOMP);
    alarm(5);
    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0 || seccomp_load(ctx) != 0) _exit(CIKIS_SISTEM);

    long r;
    if (yol == YOL_INT80) r = int80_write(MARKER, sizeof MARKER - 1);
    else r = syscall(0x40000000L + SYS_write, 1, MARKER, sizeof MARKER - 1);
    int e = errno;
    _exit((yol == YOL_INT80 ? r == -EPERM : (r == -1 && e == EPERM)) ? CIKIS_EPERM : CIKIS_BASKA);
}

static int dene(const izin_listesi *iz, enum ihlal_modu mod, enum yol yol, const char *ad)
{
    int pf[2];
    if (pipe(pf) < 0) { perror("pipe"); return 0; }
    fflush(stdout);
    pid_t p = fork();
    if (p < 0) { perror("fork"); return 0; }
    if (p == 0) {
        dup2(pf[1], 1);
        close(pf[0]);
        close(pf[1]);
        cocuk(iz, mod, yol);
    }
    close(pf[1]);
    char cikti[256] = "";
    ssize_t n = read(pf[0], cikti, sizeof cikti - 1);
    if (n < 0) n = 0;
    cikti[n] = '\0';
    close(pf[0]);
    int st;
    waitpid(p, &st, 0);

    int calisti = strstr(cikti, "ABI-CAGRISI-CALISTI") != NULL;
    int yakalandi = strstr(cikti, "SIGSYS-YAKALANDI") != NULL;
    const char *mad = mod == MOD_KILL ? "kill " : "errno";
    int ok;
    char sonuc[96];
    if (WIFSIGNALED(st)) snprintf(sonuc, sizeof sonuc, "sinyal %d (%s)", WTERMSIG(st), strsignal(WTERMSIG(st)));
    else snprintf(sonuc, sizeof sonuc, "cikis %d", WEXITSTATUS(st));

    if (mod == MOD_KILL)
        ok = WIFSIGNALED(st) && WTERMSIG(st) == SIGSYS && !calisti && !yakalandi;
    else /* foreign ABI is fatal even in errno mode (BADARCH=KILL_PROCESS); EPERM would also be acceptable */
        ok = !calisti && !yakalandi && ((WIFSIGNALED(st) && WTERMSIG(st) == SIGSYS) ||
                                        (WIFEXITED(st) && WEXITSTATUS(st) == CIKIS_EPERM));
    printf("[arch] mod=%s yol=%-12s -> %-30s cagri_calisti=%s handler_calisti=%s : %s\n", mad, ad, sonuc,
           calisti ? "EVET" : "hayir", yakalandi ? "EVET" : "hayir", ok ? "YAKALANDI" : "ATLATILDI");
    return ok;
}

int main(int argc, char **argv)
{
    const char *profil = NULL;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--profil") && i + 1 < argc) profil = argv[++i];
        else { fprintf(stderr, "kullanim: arch_testi --profil dosya.seccomp\n"); return CIKIS_KULLANIM; }
    }
    if (!profil) { fprintf(stderr, "arch_testi: --profil gerekli\n"); return CIKIS_KULLANIM; }
    izin_listesi iz = { 0 };
    if (izin_oku(profil, &iz) < 0) return CIKIS_PROFIL;
    if (!izin_var(&iz, "write") || !izin_var(&iz, "exit_group")) {
        fprintf(stderr, "arch_testi: profilde write ve exit_group izinli olmali (test onlari kullanir)\n");
        izin_serbest(&iz);
        return CIKIS_KULLANIM;
    }
    int hepsi = 1;
    for (int m = 0; m < 2; m++) {
        enum ihlal_modu mod = m ? MOD_ERRNO : MOD_KILL;
        hepsi &= dene(&iz, mod, YOL_X32, "x32");
        hepsi &= dene(&iz, mod, YOL_INT80, "int80/i386");
        if (mod == MOD_KILL) hepsi &= dene(&iz, mod, YOL_X32_HANDLER, "x32+handler");
    }
    izin_serbest(&iz);
    printf("[arch] genel: %s\n", hepsi ? "BASARILI (hicbir ABI yolu filtreyi atlatamadi)" : "BASARISIZ (atlatma var)");
    return hepsi ? CIKIS_OK : 1;
}
