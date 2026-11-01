/* ortak.h - shared definitions for hedef, sandbox and tools.
 * Kept intentionally small in Faz 0; extended in Faz 2. */
#ifndef ORTAK_H
#define ORTAK_H

#define PROJE_ADI    "seccomp-izin-listesi"
#define PROJE_SURUM  "0.1.0"

/* Exit codes shared by all binaries so tests can assert on them. */
enum ortak_cikis {
    CIKIS_OK          = 0,
    CIKIS_KULLANIM    = 2,  /* bad command line */
    CIKIS_SISTEM      = 3,  /* syscall / libc failure outside the sandbox */
    CIKIS_SECCOMP     = 4,  /* filter build or load failure */
    CIKIS_PROFIL      = 5,  /* profile file missing or malformed */
    CIKIS_EXEC        = 127 /* sandbox could not exec the command */
};

/* Policy applied to syscalls that are not on the allow list. */
enum ihlal_modu {
    MOD_KILL  = 0,  /* SCMP_ACT_KILL_PROCESS -> SIGSYS, core-like exit */
    MOD_ERRNO = 1,  /* SCMP_ACT_ERRNO(EPERM) -> call fails, process lives */
    MOD_LOG   = 2,  /* SCMP_ACT_LOG -> allowed but audited (profiling aid) */
    MOD_SAY   = 3,  /* SCMP_ACT_TRACE + ptrace: count/name blocked calls, EPERM */
    MOD_YOK   = 4   /* no filter (baseline for measurements) */
};

#endif /* ORTAK_H */
