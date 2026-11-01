/* json_mini.h - small strict JSON reader/writer for the profile schema.
 *
 * Schema (surum 1):
 *   { "surum": 1, "arch": "x86_64", "kaynak": "...", "toplam_cagri": N,
 *     "syscalls": [ {"ad": "read", "cagri": 3, "hata": 0}, ... ] }
 *
 * The reader validates full JSON syntax (so truncated or garbled files are
 * rejected) and extracts only the "syscalls" array.
 */
#ifndef JSON_MINI_H
#define JSON_MINI_H

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SC_AD_MAX 48

typedef struct {
    char ad[SC_AD_MAX];
    long cagri;
    long hata;
} sc_kayit;

typedef struct {
    sc_kayit *k;
    size_t n, kap;
} sc_liste;

static inline void sc_serbest(sc_liste *l)
{
    free(l->k);
    l->k = NULL;
    l->n = l->kap = 0;
}

/* Add count for `ad`, merging with an existing entry. 0 ok, -1 oom. */
static inline int sc_ekle(sc_liste *l, const char *ad, long cagri, long hata)
{
    for (size_t i = 0; i < l->n; i++) {
        if (strcmp(l->k[i].ad, ad) == 0) {
            /* saturate instead of overflowing (signed overflow is UB) */
            l->k[i].cagri = l->k[i].cagri > LONG_MAX - cagri ? LONG_MAX : l->k[i].cagri + cagri;
            l->k[i].hata = l->k[i].hata > LONG_MAX - hata ? LONG_MAX : l->k[i].hata + hata;
            return 0;
        }
    }
    if (l->n == l->kap) {
        size_t nk = l->kap ? l->kap * 2 : 64;
        sc_kayit *p = realloc(l->k, nk * sizeof *p);
        if (!p) return -1;
        l->k = p;
        l->kap = nk;
    }
    snprintf(l->k[l->n].ad, SC_AD_MAX, "%s", ad);
    l->k[l->n].cagri = cagri;
    l->k[l->n].hata = hata;
    l->n++;
    return 0;
}

static inline int sc_karsilastir(const void *a, const void *b)
{
    return strcmp(((const sc_kayit *)a)->ad, ((const sc_kayit *)b)->ad);
}

static inline void sc_sirala(sc_liste *l)
{
    if (l->n > 1) qsort(l->k, l->n, sizeof *l->k, sc_karsilastir);
}

static inline int sc_ad_gecerli(const char *s)
{
    if (!*s || strlen(s) >= SC_AD_MAX) return 0;
    for (; *s; s++)
        if (!(islower((unsigned char)*s) || isdigit((unsigned char)*s) || *s == '_')) return 0;
    return 1;
}

/* ---------------- reader ---------------- */
typedef struct {
    const char *p, *son, *bas;
    char hata[96];
} js_oku;

static inline int js_hata(js_oku *j, const char *m)
{
    if (!j->hata[0]) snprintf(j->hata, sizeof j->hata, "%s (bayt %ld)", m, (long)(j->p - j->bas));
    return -1;
}

static inline void js_bosluk(js_oku *j)
{
    while (j->p < j->son && (*j->p == ' ' || *j->p == '\t' || *j->p == '\n' || *j->p == '\r')) j->p++;
}

static inline int js_dize(js_oku *j, char *cikti, size_t kap)
{
    size_t n = 0;
    if (j->p >= j->son || *j->p != '"') return js_hata(j, "dize bekleniyordu");
    j->p++;
    while (j->p < j->son && *j->p != '"') {
        unsigned char c = (unsigned char)*j->p++;
        if (c < 0x20) return js_hata(j, "dizede kontrol karakteri");
        if (c == '\\') {
            if (j->p >= j->son) return js_hata(j, "kesik kacis");
            c = (unsigned char)*j->p++;
            if (c == 'u') {
                for (int i = 0; i < 4; i++)
                    if (j->p >= j->son || !isxdigit((unsigned char)*j->p++)) return js_hata(j, "gecersiz \\u");
                c = '?';
            } else if (!strchr("\"\\/bfnrt", c)) {
                return js_hata(j, "gecersiz kacis");
            }
        }
        if (cikti && n + 1 < kap) cikti[n++] = (char)c;
        else if (cikti) return js_hata(j, "dize cok uzun");
    }
    if (j->p >= j->son) return js_hata(j, "dize kapanmadi");
    j->p++;
    if (cikti) cikti[n] = '\0';
    return 0;
}

static inline int js_sayi(js_oku *j, long *deger)
{
    const char *b = j->p;
    if (j->p < j->son && *j->p == '-') j->p++;
    if (j->p >= j->son || !isdigit((unsigned char)*j->p)) return js_hata(j, "sayi bekleniyordu");
    if (*j->p == '0' && j->p + 1 < j->son && isdigit((unsigned char)j->p[1])) return js_hata(j, "basta sifir");
    while (j->p < j->son && isdigit((unsigned char)*j->p)) j->p++;
    int tam = 1;
    if (j->p < j->son && *j->p == '.') {
        tam = 0;
        j->p++;
        if (j->p >= j->son || !isdigit((unsigned char)*j->p)) return js_hata(j, "kesir hanesi yok");
        while (j->p < j->son && isdigit((unsigned char)*j->p)) j->p++;
    }
    if (j->p < j->son && (*j->p == 'e' || *j->p == 'E')) {
        tam = 0;
        j->p++;
        if (j->p < j->son && (*j->p == '+' || *j->p == '-')) j->p++;
        if (j->p >= j->son || !isdigit((unsigned char)*j->p)) return js_hata(j, "us hanesi yok");
        while (j->p < j->son && isdigit((unsigned char)*j->p)) j->p++;
    }
    if (deger) *deger = tam ? strtol(b, NULL, 10) : 0;
    return 0;
}

/* Counter field: must be a plain non-negative integer that fits in long. */
static inline int js_tam_sayi(js_oku *j, long *deger)
{
    const char *b = j->p;
    long v = 0;
    if (js_sayi(j, &v) < 0) return -1;
    for (const char *c = b; c < j->p; c++)
        if (*c == '.' || *c == 'e' || *c == 'E') return js_hata(j, "sayac tam sayi olmali");
    errno = 0;
    v = strtol(b, NULL, 10);
    if (errno == ERANGE) return js_hata(j, "sayac tasti");
    *deger = v;
    return 0;
}

/* Key reader: keys longer than `kap` simply never match ("" is returned). */
static inline int js_anahtar(js_oku *j, char *cikti, size_t kap)
{
    const char *b = j->p;
    if (js_dize(j, NULL, 0) < 0) return -1;
    size_t n = (size_t)(j->p - b) - 2;
    if (n + 1 > kap || memchr(b, '\\', n + 2)) { cikti[0] = '\0'; return 0; }
    memcpy(cikti, b + 1, n);
    cikti[n] = '\0';
    return 0;
}

static inline int js_deger(js_oku *j, int derinlik);

static inline int js_kelime(js_oku *j, const char *k)
{
    size_t n = strlen(k);
    if ((size_t)(j->son - j->p) < n || memcmp(j->p, k, n) != 0) return js_hata(j, "gecersiz sabit");
    j->p += n;
    return 0;
}

/* Generic value skipper that still validates syntax. */
static inline int js_deger(js_oku *j, int derinlik)
{
    if (derinlik > 32) return js_hata(j, "cok derin");
    js_bosluk(j);
    if (j->p >= j->son) return js_hata(j, "beklenmedik son");
    switch (*j->p) {
    case '"': return js_dize(j, NULL, 0);
    case 't': return js_kelime(j, "true");
    case 'f': return js_kelime(j, "false");
    case 'n': return js_kelime(j, "null");
    case '[':
        j->p++;
        js_bosluk(j);
        if (j->p < j->son && *j->p == ']') { j->p++; return 0; }
        for (;;) {
            if (js_deger(j, derinlik + 1) < 0) return -1;
            js_bosluk(j);
            if (j->p < j->son && *j->p == ',') { j->p++; continue; }
            if (j->p < j->son && *j->p == ']') { j->p++; return 0; }
            return js_hata(j, "',' veya ']' bekleniyordu");
        }
    case '{':
        j->p++;
        js_bosluk(j);
        if (j->p < j->son && *j->p == '}') { j->p++; return 0; }
        for (;;) {
            js_bosluk(j);
            if (js_dize(j, NULL, 0) < 0) return -1;
            js_bosluk(j);
            if (j->p >= j->son || *j->p++ != ':') return js_hata(j, "':' bekleniyordu");
            if (js_deger(j, derinlik + 1) < 0) return -1;
            js_bosluk(j);
            if (j->p < j->son && *j->p == ',') { j->p++; continue; }
            if (j->p < j->son && *j->p == '}') { j->p++; return 0; }
            return js_hata(j, "',' veya '}' bekleniyordu");
        }
    default:
        return js_sayi(j, NULL);
    }
}

/* One element of "syscalls": {"ad":..,"cagri":..,"hata":..} */
static inline int js_kayit(js_oku *j, sc_liste *l)
{
    char ad[SC_AD_MAX] = "", anahtar[32];
    long cagri = 0, hata = 0;
    int ad_var = 0;

    js_bosluk(j);
    if (j->p >= j->son || *j->p++ != '{') return js_hata(j, "syscall nesnesi bekleniyordu");
    js_bosluk(j);
    if (j->p < j->son && *j->p == '}') return js_hata(j, "bos syscall nesnesi");
    for (;;) {
        js_bosluk(j);
        if (js_anahtar(j, anahtar, sizeof anahtar) < 0) return -1;
        js_bosluk(j);
        if (j->p >= j->son || *j->p++ != ':') return js_hata(j, "':' bekleniyordu");
        js_bosluk(j);
        if (!strcmp(anahtar, "ad")) {
            if (js_dize(j, ad, sizeof ad) < 0) return -1;
            ad_var = 1;
        } else if (!strcmp(anahtar, "cagri")) {
            if (js_tam_sayi(j, &cagri) < 0) return -1;
        } else if (!strcmp(anahtar, "hata")) {
            if (js_tam_sayi(j, &hata) < 0) return -1;
        } else if (js_deger(j, 2) < 0) {
            return -1;
        }
        js_bosluk(j);
        if (j->p < j->son && *j->p == ',') { j->p++; continue; }
        if (j->p < j->son && *j->p == '}') { j->p++; break; }
        return js_hata(j, "',' veya '}' bekleniyordu");
    }
    if (!ad_var || !sc_ad_gecerli(ad)) return js_hata(j, "gecersiz veya eksik syscall adi");
    if (cagri < 0 || hata < 0) return js_hata(j, "negatif sayac");
    return sc_ekle(l, ad, cagri, hata) < 0 ? js_hata(j, "bellek yetersiz") : 0;
}

/* Header fields of a profile (optional out-parameter of js_profil_oku). */
typedef struct {
    long surum;
    char arch[32], libseccomp[32];
    int surum_var, arch_var, lib_var;
} js_meta;

/* Parse a whole profile document. 0 ok, -1 error (message in j->hata). */
static inline int js_profil_oku(const char *metin, size_t n, sc_liste *l, js_meta *meta, char *hata, size_t hata_boy)
{
    js_oku j = { .p = metin, .son = metin + n, .bas = metin, .hata = "" };
    char anahtar[32];
    int syscalls_var = 0;

    js_bosluk(&j);
    if (j.p >= j.son || *j.p++ != '{') { js_hata(&j, "kok nesne bekleniyordu"); goto bitir; }
    js_bosluk(&j);
    if (j.p < j.son && *j.p == '}') { js_hata(&j, "bos nesne"); goto bitir; }
    for (;;) {
        js_bosluk(&j);
        if (js_anahtar(&j, anahtar, sizeof anahtar) < 0) goto bitir;
        js_bosluk(&j);
        if (j.p >= j.son || *j.p++ != ':') { js_hata(&j, "':' bekleniyordu"); goto bitir; }
        js_bosluk(&j);
        if (!strcmp(anahtar, "syscalls")) {
            if (j.p >= j.son || *j.p++ != '[') { js_hata(&j, "'syscalls' dizi olmali"); goto bitir; }
            syscalls_var = 1;
            js_bosluk(&j);
            if (j.p < j.son && *j.p == ']') {
                j.p++;
            } else {
                for (;;) {
                    if (js_kayit(&j, l) < 0) goto bitir;
                    js_bosluk(&j);
                    if (j.p < j.son && *j.p == ',') { j.p++; continue; }
                    if (j.p < j.son && *j.p == ']') { j.p++; break; }
                    js_hata(&j, "',' veya ']' bekleniyordu");
                    goto bitir;
                }
            }
        } else if (meta && !strcmp(anahtar, "surum")) {
            if (js_tam_sayi(&j, &meta->surum) < 0) goto bitir;
            meta->surum_var = 1;
        } else if (meta && !strcmp(anahtar, "arch")) {
            if (js_dize(&j, meta->arch, sizeof meta->arch) < 0) goto bitir;
            meta->arch_var = 1;
        } else if (meta && !strcmp(anahtar, "libseccomp")) {
            if (js_dize(&j, meta->libseccomp, sizeof meta->libseccomp) < 0) goto bitir;
            meta->lib_var = 1;
        } else if (js_deger(&j, 1) < 0) {
            goto bitir;
        }
        js_bosluk(&j);
        if (j.p < j.son && *j.p == ',') { j.p++; continue; }
        if (j.p < j.son && *j.p == '}') { j.p++; break; }
        js_hata(&j, "',' veya '}' bekleniyordu");
        goto bitir;
    }
    js_bosluk(&j);
    if (j.p != j.son) { js_hata(&j, "belge sonunda fazlalik"); goto bitir; }
    if (!syscalls_var) js_hata(&j, "'syscalls' alani yok");
bitir:
    if (j.hata[0]) {
        snprintf(hata, hata_boy, "%s", j.hata);
        return -1;
    }
    return 0;
}

/* Read a whole file into a malloc'd buffer (NUL-terminated). */
static inline char *dosya_oku(const char *yol, size_t *n)
{
    FILE *f = fopen(yol, "rb");
    if (!f) return NULL;
    size_t kap = 4096, len = 0;
    char *b = malloc(kap);
    while (b) {
        size_t r = fread(b + len, 1, kap - len - 1, f);
        len += r;
        if (len + 1 < kap) break;
        char *nb = realloc(b, kap * 2);
        if (!nb) { free(b); b = NULL; break; }
        b = nb;
        kap *= 2;
    }
    int hata = ferror(f);
    fclose(f);
    if (!b || hata) { free(b); return NULL; }
    b[len] = '\0';
    *n = len;
    return b;
}

#endif /* JSON_MINI_H */
