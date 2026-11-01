# 00 — Proje Künyesi

| Alan | Değer |
|---|---|
| Ders | BMU1301 İşletim Sistemleri (2026–2027 Güz) |
| Proje no | #018 |
| Grup | G |
| Tema | Güvenlik |
| Zorluk | z3 |
| Başlık | seccomp ile System Call İzin Listesi |
| Ortam | Linux (Ubuntu 24.04 VM veya WSL2) |
| Araçlar | C11, libseccomp, strace, make, gcc |
| İlgili haftalar | H1 (system call arayüzü), H14 (güvenlik) |
| Depo | https://github.com/MustafaKemal0146/isletim-sistemi-homework |

## Katalog tanımı
Bir programın kullandığı system call'ları çıkarıp yalnız onlara izin vermek; seccomp-bpf.

**Büküm:** İzin listesini program çalışırken kendiliğinden çıkaran bir araç; profilin kapattığı
system call sayısının sayımı; izinsiz bir çağrıda programın nasıl sonlandığı ve günlükte nasıl
göründüğünün gösterimi.

## Takvim ve puanlama
| Aşama | Tarih | Teslim | Puan |
|---|---|---|---|
| K1 | 12.11.2026 | Heilmeier + haftalık plan + risk/B planı + açık depo | 10 |
| K2 | 26.11.2026 | Ortam + ilk ölçüm (5 tekrar) | 10 |
| K3 | 10.12.2026 | Çalışan çekirdek, ≤3 dk video, `v0.5` etiketi, ara rapor | 15 |
| K4/K5 | Final | F1 çalışırlık+büküm 20 · F2 İS derinliği 15 · F3 ölçüm 10 · F4 tekrar üretilebilirlik 10 · F5 demo+kırılma 10 | 65 |

Bireysel puan = proje puanı × savunma katsayısı (0,50–1,00).
B⁺ = (Bireysel/100) × 20 × (Yazılı final/100); Nihai = Final + B⁺ (en çok 100).

## Geliştirme ortamı (ilk kayıt, 01.11.2026)
Komut: `uname -a; gcc --version; ldd --version; strace -V`
```
Linux 7.0.0-42-generic #42~24.04.1-Ubuntu SMP PREEMPT_DYNAMIC x86_64
gcc (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0
ldd (Ubuntu GLIBC 2.39-0ubuntu8.9) 2.39
strace 6.8 ; libseccomp2 2.5.5 (runtime) — libseccomp-dev henüz kurulu değil
/proc/sys/kernel/seccomp/actions_avail: kill_process kill_thread trap errno user_notif trace log allow
kernel.dmesg_restrict=1, kernel.yama.ptrace_scope=1
```
Not: Geliştirme makinesi Ubuntu 24.04 tabanlı (Zorin OS 18). Resmi ölçümler temiz bir
Ubuntu 24.04 VM/konteynerinde tekrarlanacaktır (bkz. risk R7).
