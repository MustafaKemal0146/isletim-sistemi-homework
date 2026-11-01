# K3 — Ara Rapor (taslak, 01.11.2026 durumu)

> K3 teslimi 10.12.2026. Bu belge o güne kadar güncellenecek; aşağıdaki **"Yarım kalanlar"**
> bölümü K3'e kadar yapılacak gerçek işin listesidir.

## 1. Çalışan çekirdek
`bash scripts/calistir.sh` (kanıt: çıktı kayıtlı, `make test` ile aynı bileşenler):
profil çıkar (3 senaryo) → birleştir → `profiller/hedef.seccomp` → `sandbox --mod kill` ile `hedef tam`
başarılı → `ihlal_testi` iki modda BASARILI.

## 2. Testler
`make test`: 7 betik, **28 test grubu (T1–T27, T9b), 117 kontrol, hepsi yeşil** (`olcum/2026-11-01-test.txt`). `make asan` (`-asan.txt`) yalnızca profil/filtre/hata testlerini kapsar; ptrace döngüsü (SAY) ve sandbox altı testler sanitizer'sız çalışır.
`make` sıfır uyarı (`-Wall -Wextra -Werror -Wshadow -Wconversion -Wpedantic`).

| Konu | Test |
|---|---|
| golden profil, canlı iz, birleştirme | T1–T4 |
| deterministik filtre, bicim, bilinmeyen ad | T5–T9 |
| izinli çalışma, SIGSYS, EPERM, LOG, SAY, strace kanıtı, NO_NEW_PRIVS sırası | T10–T17 |
| bozuk/eksik girdi, boş profil, komut yok | T18–T21 |
| ölçüm betiği CSV/özet | T22 |

## 3. İhlal kanıtı
KILL (`sandbox --mod kill ... hedef ihlal`): `sinyal 31 (Bad system call)`, çıkış kodu **159**.
`strace -f` çıktısı:
```
66888 unshare(0)                        = 272
66888 +++ killed by SIGSYS +++
```
Kernel audit kaydı (`journalctl -k`, root'suz erişilebildi; `dmesg` ise `Operation not permitted`):
```
audit: type=1326 audit(...): uid=1000 pid=66977 comm="hedef" exe=".../bin/hedef" sig=31 arch=c000003e syscall=272 compat=0 ... code=0x80000000
```
`syscall=272` = `unshare`, `sig=31` = SIGSYS, `code=0x80000000` = `SECCOMP_RET_KILL_PROCESS`.
ERRNO modunda: `hedef: ihlal unshare ret=-1 errno=1 (Operation not permitted)`, süreç yaşar, çıkış 0.
SAY modunda: `engellenen system call sayisi: 1` / `engellendi: unshare (nr=272) x1 -> EPERM`.

## 4. Bulgu: `strace -c` yetersiz
İlk sürüm `strace -c` özet tablosunu ayrıştırıyordu; profille hedef SIGSYS ile öldü. Neden: tablo
`exit`/`exit_group` gibi hiç dönmeyen çağrıları saymıyor (golden fixture'da yok, canlı izde var).
Çözüm: tam iz ayrıştırma; sınır testle sabitlendi (T3).

## 5. Yarım kalanlar (K3 öncesi yapılacak)
- [x] ≤ 3 dk demo videosu hazır ve YouTube'a yüklendi: https://youtu.be/lY2M5FW9Pzk (172,84 sn; Remotion + ElevenLabs; kayıt sahnesi `demo/demo-senaryosu.md`)
- [ ] `v0.5` etiketi (K3 tarihinde, gerçek commit geçmişiyle)
- [x] Kırılma testleri yapıldı (v0.2): bkz. `rapor/final-rapor.md` §6
- [ ] LOG modunun audit kaydı (`sig=0`) örneği
- [ ] Temiz ortamda (Docker) test koşusunun sonucu
