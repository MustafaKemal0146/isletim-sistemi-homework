/* profil.c - build a syscall profile (JSON) from strace output.
 *
 * Live runs parse the full `strace -f` trace; `--strace-girdi` parses the
 * summary table of `strace -c` (used for golden tests). The trace is the
 * default because `-c` omits calls that never return (exit_group, thread exit).
 *
 *   profil [-n N] -o out.json -- cmd args...     run cmd N times under strace, union
 *   profil -o out.json --strace-girdi f.txt      parse an existing `strace -c` table
 *   profil --birlestir -o out.json a.json b.json union of profiles
 *
 * Output is sorted by syscall name so equal input gives equal bytes.
 */
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "json_mini.h"
#include "ortak.h"

static int kullanim(void)
{
    fprintf(stderr,
            "kullanim: profil [-n N] -o cikti.json -- komut [arg...]\n"
            "          profil -o cikti.json --strace-girdi dosya.txt\n"
            "          profil --birlestir -o cikti.json a.json b.json...\n");
    return CIKIS_KULLANIM;
}

/* Parse the table printed by `strace -c`. Returns number of rows or -1. */
static int strace_tablosu(const char *yol, sc_liste *l)
{
    FILE *f = fopen(yol, "r");
    if (!f) {
        fprintf(stderr, "profil: '%s' acilamadi: %s\n", yol, strerror(errno));
        return -1;
    }
    char satir[512];
    int satirlar = 0;
    while (fgets(satir, sizeof satir, f)) {
        char *tok[8];
        int nt = 0;
        for (char *s = strtok(satir, " \t\r\n"); s && nt < 8; s = strtok(NULL, " \t\r\n")) tok[nt++] = s;
        /* data row: pct seconds usecs calls [errors] name  -> 5 or 6 tokens */
        if (nt < 5 || nt > 6 || !isdigit((unsigned char)tok[0][0]) || !(strchr(tok[0], '.') || strchr(tok[0], ','))) continue;
        if (strcmp(tok[nt - 1], "total") == 0) continue;
        char *e1, *e2 = NULL;
        long cagri = strtol(tok[3], &e1, 10);
        long hata = nt == 6 ? strtol(tok[4], &e2, 10) : 0;
        if (*e1 || (nt == 6 && *e2) || !sc_ad_gecerli(tok[nt - 1])) continue;
        if (sc_ekle(l, tok[nt - 1], cagri, hata) < 0) { fclose(f); return -1; }
        satirlar++;
    }
    fclose(f);
    return satirlar;
}

/* Parse a full `strace -f` trace (-o file). Unlike the `-c` table this also sees
 * calls that never return (exit, exit_group), which a profile must contain. */
static int strace_izi(const char *yol, sc_liste *l)
{
    FILE *f = fopen(yol, "r");
    if (!f) {
        fprintf(stderr, "profil: '%s' acilamadi: %s\n", yol, strerror(errno));
        return -1;
    }
    char *satir = NULL;
    size_t kap = 0;
    ssize_t len;
    int adet = 0;
    while ((len = getline(&satir, &kap, f)) > 0) {
        char *p = satir;
        while (*p && isdigit((unsigned char)*p)) p++; /* pid prefix (-f) */
        if (p == satir) continue;
        while (*p == ' ') p++;
        char ad[SC_AD_MAX];
        int devam = 0;
        if (strncmp(p, "<... ", 5) == 0) {
            const char *b = p + 5, *e = strstr(b, " resumed>");
            if (!e || (size_t)(e - b) >= sizeof ad) continue;
            memcpy(ad, b, (size_t)(e - b));
            ad[e - b] = '\0';
            devam = 1;
        } else {
            const char *b = p;
            while (islower((unsigned char)*p) || isdigit((unsigned char)*p) || *p == '_') p++;
            if (p == b || *p != '(' || (size_t)(p - b) >= sizeof ad) continue;
            memcpy(ad, b, (size_t)(p - b));
            ad[p - b] = '\0';
        }
        if (!sc_ad_gecerli(ad)) continue;
        while (len > 0 && (satir[len - 1] == '\n' || satir[len - 1] == ' ')) satir[--len] = '\0';
        int unfinished = len >= 12 && strcmp(satir + len - 12, "<unfinished ...>") == 0 ? 1 : 0;
        int hata = !unfinished && strstr(satir, ") = -1 E") != NULL ? 1 : 0;
        if (sc_ekle(l, ad, devam ? 0 : 1, hata) < 0) { free(satir); fclose(f); return -1; }
        adet++;
    }
    free(satir);
    fclose(f);
    return adet;
}

/* Run `strace -f` once on cmd; the target's own output is discarded. */
static int strace_calistir(char *const *komut, const char *tablo)
{
    pid_t p = fork();
    if (p < 0) { perror("fork"); return -1; }
    if (p == 0) {
        int dn = open("/dev/null", O_RDWR);
        if (dn >= 0) { dup2(dn, 1); dup2(dn, 2); }
        size_t n = 0;
        while (komut[n]) n++;
        char **av = calloc(n + 8, sizeof *av);
        if (!av) _exit(CIKIS_SISTEM);
        size_t k = 0;
        av[k++] = "strace"; av[k++] = "-f"; av[k++] = "-qq"; av[k++] = "-s"; av[k++] = "0";
        av[k++] = "-o"; av[k++] = (char *)tablo; av[k++] = "--";
        for (size_t i = 0; i < n; i++) av[k++] = komut[i];
        setenv("LC_ALL", "C", 1); /* decimal point in the table must not depend on locale */
        execvp("strace", av);
        _exit(CIKIS_EXEC);
    }
    int st;
    if (waitpid(p, &st, 0) < 0) { perror("waitpid"); return -1; }
    if (WIFEXITED(st) && WEXITSTATUS(st) == CIKIS_EXEC) {
        fprintf(stderr, "profil: strace calistirilamadi (kurulu mu? ptrace izni var mi?)\n");
        return -1;
    }
    /* strace exits with the target's status; a failing target is still profiled,
     * but a profile from a failed run under-allows, so say so. */
    if (!WIFEXITED(st) || WEXITSTATUS(st) != 0)
        fprintf(stderr, "profil: UYARI: hedef basarisiz bitti (durum 0x%x); profil eksik olabilir\n", (unsigned)st);
    return 0;
}

static int yaz(const char *yol, const sc_liste *l, const char *kaynak)
{
    FILE *f = fopen(yol, "w");
    if (!f) {
        fprintf(stderr, "profil: '%s' yazilamadi: %s\n", yol, strerror(errno));
        return -1;
    }
    long toplam = 0;
    for (size_t i = 0; i < l->n; i++) toplam += l->k[i].cagri;
    fprintf(f, "{\n  \"surum\": 1,\n  \"arch\": \"x86_64\",\n  \"kaynak\": \"");
    for (const char *c = kaynak; *c; c++) {
        if (*c == '"' || *c == '\\') fputc('\\', f);
        if ((unsigned char)*c >= 0x20) fputc(*c, f);
    }
    fprintf(f, "\",\n  \"toplam_cagri\": %ld,\n  \"farkli_syscall\": %zu,\n  \"syscalls\": [\n", toplam, l->n);
    for (size_t i = 0; i < l->n; i++)
        fprintf(f, "    {\"ad\": \"%s\", \"cagri\": %ld, \"hata\": %ld}%s\n", l->k[i].ad,
                l->k[i].cagri, l->k[i].hata, i + 1 < l->n ? "," : "");
    fprintf(f, "  ]\n}\n");
    if (fclose(f) != 0) { perror("fclose"); return -1; }
    return 0;
}

int main(int argc, char **argv)
{
    const char *cikti = NULL, *girdi = NULL;
    int birlestir = 0, tekrar = 1, i = 1;
    char *const *komut = NULL;

    for (; i < argc; i++) {
        if (!strcmp(argv[i], "-o") && i + 1 < argc) cikti = argv[++i];
        else if (!strcmp(argv[i], "-n") && i + 1 < argc) {
            tekrar = atoi(argv[++i]);
            if (tekrar < 1 || tekrar > 100) return kullanim();
        } else if (!strcmp(argv[i], "--strace-girdi") && i + 1 < argc) girdi = argv[++i];
        else if (!strcmp(argv[i], "--birlestir")) birlestir = 1;
        else if (!strcmp(argv[i], "--")) { komut = &argv[i + 1]; i = argc; break; }
        else if (argv[i][0] == '-') return kullanim();
        else break;
    }
    if (!cikti) return kullanim();

    sc_liste l = { 0 };
    char kaynak[256] = "";
    int rc = CIKIS_OK;

    if (birlestir) {
        if (i >= argc) return kullanim();
        int nd = 0;
        for (; i < argc; i++, nd++) {
            size_t n;
            char *m = dosya_oku(argv[i], &n);
            char hata[128];
            if (!m) {
                fprintf(stderr, "profil: '%s' okunamadi: %s\n", argv[i], strerror(errno));
                rc = CIKIS_PROFIL;
                goto cik;
            }
            if (js_profil_oku(m, n, &l, hata, sizeof hata) < 0) {
                fprintf(stderr, "profil: '%s' bozuk JSON: %s\n", argv[i], hata);
                free(m);
                rc = CIKIS_PROFIL;
                goto cik;
            }
            free(m);
        }
        snprintf(kaynak, sizeof kaynak, "birlesim:%d profil", nd);
    } else if (girdi) {
        if (strace_tablosu(girdi, &l) <= 0) {
            fprintf(stderr, "profil: '%s' icinde strace -c tablosu bulunamadi\n", girdi);
            rc = CIKIS_PROFIL;
            goto cik;
        }
        snprintf(kaynak, sizeof kaynak, "strace-girdi");
    } else if (komut && komut[0]) {
        char tablo[] = "/tmp/profil-strace-XXXXXX";
        for (int r = 0; r < tekrar; r++) {
            int fd = mkstemp(tablo);
            if (fd < 0) { perror("mkstemp"); rc = CIKIS_SISTEM; goto cik; }
            close(fd);
            int ok = strace_calistir(komut, tablo) == 0 && strace_izi(tablo, &l) > 0;
            unlink(tablo);
            if (!ok) {
                fprintf(stderr, "profil: strace ciktisi alinamadi (kosu %d)\n", r + 1);
                rc = CIKIS_SISTEM;
                goto cik;
            }
            snprintf(tablo, sizeof tablo, "/tmp/profil-strace-XXXXXX");
        }
        size_t o = 0;
        for (char *const *a = komut; *a && o + 2 < sizeof kaynak; a++)
            o += (size_t)snprintf(kaynak + o, sizeof kaynak - o, "%s%s", o ? " " : "", *a);
        snprintf(kaynak + (o < sizeof kaynak - 12 ? o : sizeof kaynak - 12), 12, " (x%d)", tekrar);
    } else {
        return kullanim();
    }

    sc_sirala(&l);
    if (yaz(cikti, &l, kaynak) < 0) rc = CIKIS_SISTEM;
    else fprintf(stderr, "profil: %zu farkli syscall -> %s\n", l.n, cikti);
cik:
    sc_serbest(&l);
    return rc;
}
