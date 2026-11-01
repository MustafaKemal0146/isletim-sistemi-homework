# K2 — Ortam ve İlk Ölçüm

> **Durum: ön ölçüm (01.11.2026).** K2 teslim tarihi 26.11.2026'dır; aşağıdaki ölçümler bu
> makinede alındı ve K2 öncesinde temiz Ubuntu 24.04 ortamında (Docker/VM) **tekrarlanıp bu rapor
> güncellenecek.** Her sayı `olcum/2026-11-01-olcum.csv` dosyasındaki ham verinin özetidir.

## 1. Ortam
`make surum` ve `olcum/2026-11-01-ozet.md` çıktısı:
```
Linux 7.0.0-42-generic #42~24.04.1-Ubuntu SMP PREEMPT_DYNAMIC x86_64
gcc (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0 ; glibc 2.39
libseccomp 2.5.5-1ubuntu3.1 (statik, .deps; sudo yokluğu nedeniyle)
strace 6.8 ; CPU: 13th Gen Intel(R) Core(TM) i5-13500H, nproc 16
kernel.dmesg_restrict=1, kernel.yama.ptrace_scope=1   (OS/glibc/ptrace_scope satırları: olcum/2026-11-01-ozet.md)
```
Geliştirme makinesi Zorin OS 18 (Ubuntu 24.04 tabanlı). Sudo parola istediği için
`libseccomp-dev` sistem paketi yerine `scripts/kur-rootsuz.sh` ile kuruldu (statik bağlama).

## 2. Yöntem
`docs/05-olcum-yontemi.md`: 6 senaryo, her birinde 1 ısınma + 5 ölçülen tekrar, ortalama ± örnek
standart sapma. Komut: `bash scripts/calistir.sh && TARIH=2026-11-01 bash tools/olcum.sh`.

## 3. Profil
3 senaryo (normal/hata/socket) × 3 koşu `strace -f` ile izlenip birleştirildi:
**45 izinli, 323 kapalı, toplam 368 (%87,8 daralma)** (`profil_uret --ozet`).
Determinizm: iki ayrı üretimde `sha256sum`:
```
6ac7efd2c8462c38a5a43143068be7cb051a2fbace9164bdf615d0528e21eab1  hedef.seccomp (her ikisi)
be5302bd3f3cc272b125afa80a94bbcd9e4f113afafe3bc0d5e31bd6205fde25  hedef.bpf (her ikisi)
```

## 4. Sonuçlar (ortalama ± standart sapma, n=5)

| Senaryo | syscall sayisi (ort) | engellenen (ort) | sure_ms | min–max ms | rss_kb | exit |
|---|---|---|---|---|---|---|
| yok-tam | 119.6 | 0.0 | 10.04 ± 0.50 | 9.50–10.73 | 10684 ± 71 | 0 |
| kill-tam | 120.0 | 0.0 | 10.25 ± 0.41 | 9.79–10.83 | 10659 ± 70 | 0 |
| errno-tam | 121.6 | 0.0 | 10.03 ± 0.71 | 8.98–10.79 | 10645 ± 88 | 0 |
| say-tam | 120.0 | 0.0 | 9.81 ± 0.49 | 9.08–10.26 | 10659 ± 70 | 0 |
| say-ihlal | 95.2 | 1.0 | 9.65 ± 0.72 | 8.56–10.38 | 10607 ± 91 | 0 |
| kill-ihlal | 94.6 | 1.0 | 9.73 ± 0.47 | 8.99–10.24 | 10659 ± 70 | 159 |

syscall sayisi ve engellenen: tekrarlarin ortalamasi; exit: tum tekrarlarda ayni deger. Standart sapma: ornek sapmasi (n-1). Ham veri: `olcum/2026-11-01-olcum.csv`.

![süre](../olcum/2026-11-01-sure.png)

## 5. Yorum
- **Filtre, programın yaptığı çağrıların sayısını değiştirmiyor; kümeyi daraltıyor.** (Not: `syscall_sayisi`
  sütunu her tekrarda *filtresiz* ayrı bir `strace` koşusundan geldiği için bu cümleyi tek başına kanıtlamaz;
  kanıt T10'dur: profilli ve profilsiz stdout aynı.) Tekrarlar arası oynama gerçektir (ör. errno-tam: 119–129,
  thread zamanlamasından kaynaklı `futex` sayısı).
- **Filtre ek yükü bu ölçekte gürültüden ayırt edilemiyor:** 10,04 ± 0,50 ms (filtresiz) ve
  10,25 ± 0,41 ms (KILL) farkı standart sapmadan küçük. "Ek yük yok" demiyoruz; ölçüm çözünürlüğü
  yetmiyor (n=5, ~10 ms, fork+exec baskın).
- İhlal senaryolarında `kill-ihlal` exit 159 (=128+SIGSYS), `say-ihlal` 1 engellenen çağrı
  (`unshare`) sayıyor.

## 6. Bilinen eksikler (K2'ye kalan gerçek iş)
1. Temiz Ubuntu 24.04 ortamında tekrar ve rapor güncellemesi.
2. Daha uzun koşan bir yükle (ör. çok sayıda syscall döngüsü) filtre ek yükünü ölçülebilir kılmak.
3. `yok-tam` ve `kill-tam` farkı için eşleştirilmiş (paired) karşılaştırma.
