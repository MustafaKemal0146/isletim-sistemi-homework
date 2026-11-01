/* sandbox.c - run a command under a seccomp allow-list.
 *
 *   sandbox [--mod kill|errno|log|say|yok] --profil dosya.seccomp [--olcum] -- cmd [args...]
 *
 * Order in the child (this order matters):
 *   1. prctl(PR_SET_NO_NEW_PRIVS, 1)   - required to load a filter without root
 *   2. seccomp_load(filter)
 *   3. execvp(cmd)                     - execve is part of the profile
 *
 * Modes (action for syscalls that are NOT on the allow list):
 *   kill   SCMP_ACT_KILL_PROCESS  -> SIGSYS, process dies
 *   errno  SCMP_ACT_ERRNO(EPERM)  -> call fails with EPERM, process lives
 *   log    SCMP_ACT_LOG           -> call runs, kernel audits it (dmesg/journal)
 *   say    SCMP_ACT_TRACE + ptrace-> call fails with EPERM and we count/name it
 *   yok    no filter (baseline)
 *
 * Exit status: the command's own status; 128+N if it died from signal N;
 * 2/3/4/5/127 for sandbox-side errors (see ortak.h). x86_64 only (ptrace regs).
 */
#include <errno.h>
#include <seccomp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/ptrace.h>
#include <sys/resource.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "ortak.h"
#include "profil_oku.h"

#define MAX_SYSCALL 1024

static int kullanim(void)
{
    fprintf(stderr,
            "kullanim: sandbox [--mod kill|errno|log|say|yok] [--profil dosya.seccomp] [--olcum] -- komut [arg...]\n");
    return CIKIS_KULLANIM;
}

static int mod_coz(const char *s, enum ihlal_modu *m)
{
    static const struct { const char *ad; enum ihlal_modu m; } t[] = {
        { "kill", MOD_KILL }, { "errno", MOD_ERRNO }, { "log", MOD_LOG }, { "say", MOD_SAY }, { "yok", MOD_YOK },
    };
    for (size_t i = 0; i < sizeof t / sizeof *t; i++)
        if (!strcmp(s, t[i].ad)) { *m = t[i].m; return 0; }
    return -1;
}

static double simdi_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1e6;
}

/* Child side: runs between fork and exec. Never returns. */
static void cocuk(enum ihlal_modu mod, scmp_filter_ctx ctx, char *const *komut, pid_t ana)
{
    /* If the sandbox itself is killed (kill -9), the command must not survive as an orphan. */
    if (prctl(PR_SET_PDEATHSIG, SIGKILL) != 0) _exit(CIKIS_SISTEM);
    if (getppid() != ana) _exit(CIKIS_SISTEM); /* parent died before prctl took effect */
    if (mod == MOD_SAY) {
        if (ptrace(PTRACE_TRACEME, 0, 0, 0) < 0) _exit(CIKIS_SISTEM);
        raise(SIGSTOP); /* parent sets ptrace options, then resumes us */
    }
    if (mod != MOD_YOK) {
        if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0) _exit(CIKIS_SISTEM);
        if (seccomp_load(ctx) != 0) _exit(CIKIS_SECCOMP);
    }
    execvp(komut[0], komut);
    _exit(CIKIS_EXEC); /* exec failed (or was blocked by the filter itself) */
}

/* Parent loop for `say`: follow the tracee tree, count seccomp-trace stops. */
static int izle(pid_t cocuk_pid, int *durum, struct rusage *ru, long *sayac)
{
    int st;
    pid_t p = waitpid(cocuk_pid, &st, WUNTRACED);
    if (p < 0 || !WIFSTOPPED(st)) return -1;
    long secenek = PTRACE_O_TRACESECCOMP | PTRACE_O_TRACECLONE | PTRACE_O_TRACEFORK |
                   PTRACE_O_TRACEVFORK | PTRACE_O_TRACEEXEC | PTRACE_O_EXITKILL;
    if (ptrace(PTRACE_SETOPTIONS, cocuk_pid, 0, secenek) < 0) return -1;
    ptrace(PTRACE_CONT, cocuk_pid, 0, 0);

    for (;;) {
        struct rusage r;
        pid_t w = wait4(-1, &st, __WALL, &r);
        if (w < 0) return -1;
        if (WIFEXITED(st) || WIFSIGNALED(st)) {
            if (w == cocuk_pid) { *durum = st; *ru = r; return 0; }
            continue;
        }
        if (!WIFSTOPPED(st)) continue;
        int sig = WSTOPSIG(st), olay = st >> 16, ilet = 0;
        if (olay == PTRACE_EVENT_SECCOMP) {
            struct user_regs_struct rg;
            if (ptrace(PTRACE_GETREGS, w, 0, &rg) != 0) {
                kill(w, SIGKILL); /* never fail open: the blocked call must not run */
                continue;
            }
            {
                long nr = (long)rg.orig_rax;
                if (nr >= 0 && nr < MAX_SYSCALL) sayac[nr]++;
                rg.orig_rax = (unsigned long long)-1; /* skip the real call ... */
                rg.rax = (unsigned long long)-EPERM;  /* ... and fake EPERM */
                ptrace(PTRACE_SETREGS, w, 0, &rg);
            }
        } else if (olay == 0 && sig != SIGTRAP && sig != SIGSTOP) {
            ilet = sig; /* ordinary signal for the tracee: pass it on */
        }
        ptrace(PTRACE_CONT, w, 0, ilet);
    }
}

int main(int argc, char **argv)
{
    enum ihlal_modu mod = MOD_KILL;
    const char *profil = NULL;
    int olcum = 0, i = 1;

    for (; i < argc; i++) {
        if (!strcmp(argv[i], "--mod") && i + 1 < argc) {
            if (mod_coz(argv[++i], &mod) < 0) return kullanim();
        } else if (!strcmp(argv[i], "--profil") && i + 1 < argc) profil = argv[++i];
        else if (!strcmp(argv[i], "--olcum")) olcum = 1;
        else if (!strcmp(argv[i], "--")) { i++; break; }
        else return kullanim();
    }
    if (i >= argc) return kullanim();
    char *const *komut = &argv[i];
    if (mod != MOD_YOK && !profil) {
        fprintf(stderr, "sandbox: --profil gerekli\n");
        return kullanim();
    }

    izin_listesi iz = { 0 };
    scmp_filter_ctx ctx = NULL;
    if (mod != MOD_YOK) {
        if (izin_oku(profil, &iz) < 0) return CIKIS_PROFIL;
        ctx = filtre_kur(&iz, mod_eylemi(mod));
        if (!ctx) { izin_serbest(&iz); return CIKIS_SECCOMP; }
        /* The child's failure path (exec blocked/failed) must be able to exit even
         * with an empty profile; otherwise glibc's _exit() spins and crashes. */
        const char *ek = "";
        if (!izin_var(&iz, "exit_group")) {
            if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(exit_group), 0) != 0) {
                fprintf(stderr, "sandbox: exit_group kurali eklenemedi\n");
                return CIKIS_SECCOMP;
            }
            ek = " (+exit_group)";
            fprintf(stderr, "[sandbox] not: exit_group profilde yok; sandbox hata yolu icin eklendi\n");
        }
        fprintf(stderr, "[sandbox] profil=%s izinli=%zu%s\n", profil, iz.n, ek);
    }

    static long sayac[MAX_SYSCALL];
    int durum = 0;
    struct rusage ru;
    memset(&ru, 0, sizeof ru);

    double t0 = simdi_ms();
    pid_t ana_pid = getpid();
    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return CIKIS_SISTEM; }
    if (pid == 0) cocuk(mod, ctx, komut, ana_pid);

    if (mod == MOD_SAY) {
        if (izle(pid, &durum, &ru, sayac) < 0) {
            fprintf(stderr, "sandbox: ptrace izleme basarisiz: %s\n", strerror(errno));
            return CIKIS_SISTEM;
        }
    } else if (wait4(pid, &durum, 0, &ru) < 0) {
        perror("wait4");
        return CIKIS_SISTEM;
    }
    double sure = simdi_ms() - t0;

    int cikis, sinyal = 0;
    long engellenen = 0;
    for (int n = 0; n < MAX_SYSCALL; n++) engellenen += sayac[n];

    if (WIFSIGNALED(durum)) {
        sinyal = WTERMSIG(durum);
        cikis = 128 + sinyal;
        fprintf(stderr, "[sandbox] cocuk sinyal ile sonlandi: %d (%s)%s\n", sinyal, strsignal(sinyal),
                WCOREDUMP(durum) ? " [core]" : "");
        if (sinyal == SIGSYS && mod == MOD_KILL) {
            engellenen = 1;
            fprintf(stderr, "[sandbox] SIGSYS = izin listesinde olmayan system call (seccomp KILL_PROCESS). "
                            "Cekirdek audit kaydi: type=1326 (journalctl -k / dmesg, root gerekebilir)\n");
        } else if (sinyal == SIGSYS) {
            fprintf(stderr, "[sandbox] SIGSYS ama filtre bu modda oldurmez; hedefin kendi sinyali\n");
        }
    } else {
        cikis = WEXITSTATUS(durum);
        if (cikis == CIKIS_EXEC)
            fprintf(stderr, "[sandbox] komut calistirilamadi (exec basarisiz veya filtre execve'yi engelledi)\n");
    }

    if (mod == MOD_SAY) {
        fprintf(stderr, "[sandbox] engellenen system call sayisi: %ld\n", engellenen);
        for (int n = 0; n < MAX_SYSCALL; n++) {
            if (!sayac[n]) continue;
            char *ad = seccomp_syscall_resolve_num_arch(SCMP_ARCH_NATIVE, n);
            fprintf(stderr, "[sandbox]   engellendi: %s (nr=%d) x%ld -> EPERM\n", ad ? ad : "?", n, sayac[n]);
            free(ad);
        }
    }
    if (olcum)
        fprintf(stderr, "SANDBOX_OLCUM sure_ms=%.3f rss_kb=%ld exit=%d sinyal=%d engellenen=%ld\n", sure,
                ru.ru_maxrss, cikis, sinyal, engellenen);

    if (ctx) seccomp_release(ctx);
    izin_serbest(&iz);
    return cikis;
}
