/* hedef.c - sample target that exercises many different system calls.
 *
 * Scenarios (argv[1]):
 *   normal  file I/O, memory (malloc/mmap/mprotect), clock/nanosleep, threads
 *   hata    error paths: failing open/unlink/connect, message on stderr
 *   socket  AF_UNIX socketpair + TCP over 127.0.0.1
 *   tam     normal + hata + socket
 *   ihlal   normal, then one syscall that no profile contains (unshare)
 *   uzun [sn]       long-running loop (kill -9 test)
 *   disk <dizin>    write until the filesystem is full (ENOSPC test)
 *   bellek <MB>     allocate and touch MB megabytes (MemoryMax / OOM test)
 *   hesap [milyon]  CPU-bound deterministic computation (CPUQuota test)
 *
 * stdout is deterministic so sandboxed and unsandboxed runs can be diffed.
 */
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

#include "ortak.h"

#define VERI_BOYUT (16u * 1024u)
#define IS_PARCACIK 3
#define ARTIS 1000

static uint32_t fnv1a(uint32_t h, const unsigned char *p, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        h ^= p[i];
        h *= 16777619u;
    }
    return h;
}

/* ---- normal: files ---- */
static int dosya_isi(uint32_t *sum)
{
    char dizin[] = "/tmp/hedef-XXXXXX";
    char yol[64], yol2[64];
    unsigned char buf[VERI_BOYUT];
    unsigned char oku[VERI_BOYUT];

    if (!mkdtemp(dizin)) { perror("mkdtemp"); return -1; }
    snprintf(yol, sizeof yol, "%s/veri.bin", dizin);
    snprintf(yol2, sizeof yol2, "%s/veri2.bin", dizin);
    for (size_t i = 0; i < sizeof buf; i++) buf[i] = (unsigned char)(i * 31u + 7u);

    int fd = open(yol, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0) { perror("open(yaz)"); return -1; }
    if (write(fd, buf, sizeof buf) != (ssize_t)sizeof buf) { perror("write"); close(fd); return -1; }
    if (fsync(fd) < 0) { perror("fsync"); close(fd); return -1; }
    close(fd);

    fd = open(yol, O_RDONLY);
    if (fd < 0) { perror("open(oku)"); return -1; }
    struct stat st;
    if (fstat(fd, &st) < 0) { perror("fstat"); close(fd); return -1; }
    ssize_t n = read(fd, oku, sizeof oku);
    if (n != (ssize_t)sizeof oku || lseek(fd, 0, SEEK_SET) < 0) { perror("read"); close(fd); return -1; }
    close(fd);

    *sum = fnv1a(2166136261u, oku, (size_t)n);
    if (rename(yol, yol2) < 0 || stat(yol2, &st) < 0) { perror("rename"); return -1; }
    if (unlink(yol2) < 0 || rmdir(dizin) < 0) { perror("unlink"); return -1; }
    return 0;
}

/* ---- normal: memory + time ---- */
static int bellek_zaman(unsigned long *kontrol)
{
    size_t boy = 8u * 1024u * 1024u;
    unsigned char *a = malloc(boy);
    if (!a) return -1;
    memset(a, 0xAB, boy);
    unsigned char *m = mmap(NULL, 1u << 20, PROT_READ | PROT_WRITE,
                            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (m == MAP_FAILED) { free(a); return -1; }
    memset(m, 0x5A, 1u << 20);
    if (mprotect(m, 1u << 20, PROT_READ) < 0) { munmap(m, 1u << 20); free(a); return -1; }
    *kontrol = (unsigned long)a[boy - 1] + m[100];
    munmap(m, 1u << 20);
    free(a);

    struct timespec t0, t1, bekle = { 0, 1000000 };
    clock_gettime(CLOCK_MONOTONIC, &t0);
    nanosleep(&bekle, NULL);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    (void)getpid();
    (void)time(NULL);
    return (t1.tv_sec > t0.tv_sec || t1.tv_nsec > t0.tv_nsec) ? 0 : -1;
}

/* ---- normal: threads ---- */
static pthread_mutex_t kilit = PTHREAD_MUTEX_INITIALIZER;
static unsigned long sayac;

static void *is_fonksiyonu(void *arg)
{
    (void)arg;
    for (int i = 0; i < ARTIS; i++) {
        pthread_mutex_lock(&kilit);
        sayac++;
        pthread_mutex_unlock(&kilit);
    }
    return NULL;
}

static int thread_isi(unsigned long *toplam)
{
    pthread_t t[IS_PARCACIK];
    for (int i = 0; i < IS_PARCACIK; i++)
        if (pthread_create(&t[i], NULL, is_fonksiyonu, NULL) != 0) return -1;
    for (int i = 0; i < IS_PARCACIK; i++) pthread_join(t[i], NULL);
    *toplam = sayac;
    return 0;
}

static int senaryo_normal(void)
{
    uint32_t sum = 0;
    unsigned long bz = 0, th = 0;
    if (dosya_isi(&sum) < 0 || bellek_zaman(&bz) < 0 || thread_isi(&th) < 0) {
        fprintf(stderr, "hedef: normal senaryo basarisiz\n");
        return 1;
    }
    printf("hedef: normal tamam dosya_sum=%08x bellek=%lu thread_toplam=%lu\n",
           (unsigned)sum, bz, th);
    return 0;
}

/* ---- hata: failing calls handled gracefully ---- */
static int senaryo_hata(void)
{
    int fd = open("/olmayan/dizin/dosya", O_RDONLY);
    int e1 = errno;
    fprintf(stderr, "hedef: open basarisiz: %s\n", fd < 0 ? strerror(e1) : "beklenmedik basari");

    int r = unlink("/olmayan/dizin/dosya");
    int e2 = errno;
    fprintf(stderr, "hedef: unlink basarisiz: %s\n", r < 0 ? strerror(e2) : "beklenmedik basari");

    int s = socket(AF_INET, SOCK_STREAM, 0);
    int e3 = 0;
    if (s >= 0) {
        struct sockaddr_in a = { .sin_family = AF_INET, .sin_port = htons(1) };
        a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (connect(s, (struct sockaddr *)&a, sizeof a) < 0) e3 = errno;
        close(s);
    }
    fprintf(stderr, "hedef: connect basarisiz: %s\n", e3 ? strerror(e3) : "beklenmedik basari");

    printf("hedef: hata tamam hatalar=%d,%d,%d\n", e1, e2, e3);
    return (fd < 0 && r < 0 && e3 != 0) ? 0 : 1;
}

/* ---- socket ---- */
static int senaryo_socket(void)
{
    int sp[2];
    char buf[16];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sp) < 0) { perror("socketpair"); return 1; }
    if (write(sp[0], "unix", 4) != 4 || read(sp[1], buf, sizeof buf) != 4) { perror("unix io"); return 1; }
    close(sp[0]);
    close(sp[1]);

    int ls = socket(AF_INET, SOCK_STREAM, 0);
    if (ls < 0) { perror("socket"); return 1; }
    int bir = 1;
    setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, &bir, sizeof bir);
    struct sockaddr_in a = { .sin_family = AF_INET, .sin_port = 0 };
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    socklen_t al = sizeof a;
    if (bind(ls, (struct sockaddr *)&a, sizeof a) < 0 || listen(ls, 1) < 0 ||
        getsockname(ls, (struct sockaddr *)&a, &al) < 0) { perror("bind/listen"); return 1; }

    int cs = socket(AF_INET, SOCK_STREAM, 0);
    if (cs < 0 || connect(cs, (struct sockaddr *)&a, sizeof a) < 0) { perror("connect"); return 1; }
    int as = accept(ls, NULL, NULL);
    if (as < 0) { perror("accept"); return 1; }
    if (send(cs, "merhaba", 7, 0) != 7 || recv(as, buf, sizeof buf, 0) != 7) { perror("send/recv"); return 1; }
    int ok = memcmp(buf, "merhaba", 7) == 0;
    close(as);
    close(cs);
    close(ls);
    printf("hedef: socket tamam mesaj=%s\n", ok ? "dogru" : "bozuk");
    return ok ? 0 : 1;
}

/* ---- ihlal: a call that is in no profile ---- */
static int senaryo_ihlal(void)
{
    int r = senaryo_normal();
    if (r != 0) return r;
    long ret = syscall(SYS_unshare, 0);
    int e = errno;
    printf("hedef: ihlal unshare ret=%ld errno=%d (%s)\n", ret, ret < 0 ? e : 0,
           ret < 0 ? strerror(e) : "izinli calisti");
    return 0;
}

/* ---- uzun: runs for `sn` seconds, one line per second ---- */
static int senaryo_uzun(int sn)
{
    struct timespec t = { 0, 100000000 };
    for (int i = 0; i < sn * 10; i++) {
        nanosleep(&t, NULL);
        if (i % 10 == 0) {
            printf("hedef: uzun tik %d\n", i / 10);
            fflush(stdout);
        }
    }
    printf("hedef: uzun tamam\n");
    return 0;
}

/* ---- disk: fill the directory's filesystem; ENOSPC must be reported, not crash ---- */
static int senaryo_disk(const char *dizin)
{
    char yol[256];
    static unsigned char buf[64 * 1024];
    snprintf(yol, sizeof yol, "%s/dolgu.bin", dizin);
    memset(buf, 0x5A, sizeof buf);
    int fd = open(yol, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0) { fprintf(stderr, "hedef: '%s' acilamadi: %s\n", yol, strerror(errno)); return CIKIS_SISTEM; }
    size_t toplam = 0;
    for (;;) {
        ssize_t w = write(fd, buf, sizeof buf);
        if (w < 0) {
            int e = errno;
            fprintf(stderr, "hedef: disk dolu: write basarisiz: %s (errno=%d), yazilan=%zu KB\n", strerror(e),
                    e, toplam / 1024);
            close(fd);
            unlink(yol);
            fprintf(stderr, "hedef: gecici dosya silindi, temiz cikis\n");
            return CIKIS_SISTEM;
        }
        toplam += (size_t)w;
        if (toplam > ((size_t)4096 << 20)) { /* safety: never write more than 4 GiB */
            fprintf(stderr, "hedef: guvenlik siniri asildi (disk dolmadi)\n");
            close(fd);
            unlink(yol);
            return CIKIS_SISTEM;
        }
    }
}

/* ---- bellek: touch `mb` MiB in 1 MiB chunks ---- */
static int senaryo_bellek(int mb)
{
    void **p = calloc((size_t)mb, sizeof *p);
    if (!p) return CIKIS_SISTEM;
    for (int i = 0; i < mb; i++) {
        p[i] = malloc(1u << 20);
        if (!p[i]) { fprintf(stderr, "hedef: malloc basarisiz (%d MiB)\n", i); return CIKIS_SISTEM; }
        memset(p[i], i, 1u << 20);
        if (i % 8 == 7) { printf("hedef: bellek %d MiB dokunuldu\n", i + 1); fflush(stdout); }
    }
    for (int i = 0; i < mb; i++) free(p[i]);
    free(p);
    printf("hedef: bellek tamam %d MiB\n", mb);
    return 0;
}

/* ---- hesap: xorshift64 loop, no syscalls; result must not depend on CPU limits ---- */
static int senaryo_hesap(unsigned long milyon)
{
    uint64_t x = 88172645463325252ull, toplam = 0;
    for (unsigned long i = 0; i < milyon * 1000000ul; i++) {
        x ^= x << 13;
        x ^= x >> 7;
        x ^= x << 17;
        toplam += x;
    }
    printf("hedef: hesap %luM sonuc=%016llx\n", milyon, (unsigned long long)toplam);
    return 0;
}

int main(int argc, char **argv)
{
    const char *s = argc > 1 ? argv[1] : "tam";
    int r;

    if (!strcmp(s, "normal"))      r = senaryo_normal();
    else if (!strcmp(s, "hata"))   r = senaryo_hata();
    else if (!strcmp(s, "socket")) r = senaryo_socket();
    else if (!strcmp(s, "ihlal"))  r = senaryo_ihlal();
    else if (!strcmp(s, "uzun"))   r = senaryo_uzun(argc > 2 ? atoi(argv[2]) : 30);
    else if (!strcmp(s, "disk") && argc > 2)   r = senaryo_disk(argv[2]);
    else if (!strcmp(s, "bellek") && argc > 2) r = senaryo_bellek(atoi(argv[2]));
    else if (!strcmp(s, "hesap"))  r = senaryo_hesap(argc > 2 ? strtoul(argv[2], NULL, 10) : 300);
    else if (!strcmp(s, "tam"))    r = senaryo_normal() | senaryo_hata() | senaryo_socket();
    else {
        fprintf(stderr, "kullanim: %s normal|hata|socket|tam|ihlal|uzun [sn]|disk dizin|bellek MB|hesap [milyon]\n", argv[0]);
        return CIKIS_KULLANIM;
    }
    return r ? CIKIS_SISTEM : CIKIS_OK;
}
