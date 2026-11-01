# Final Raporu (çalışma taslağı)

> Durum: 01.11.2026. Çekirdek ve ölçümler bu tarihte; **yazılı olmayan** (yapılmamış) işler §9'da
> açıkça listelenmiştir. Final teslimine kadar §6, §7 ve §9 güncellenecek.

**BMU1301 · Proje #018 · seccomp ile System Call İzin Listesi** — Mustafa Kemal Çıngıl, 24080410023

## 1. Amaç ve katkı
Bir programın system call izin listesini çalışırken otomatik çıkarmak, bunu libseccomp ile
bir BPF filtresine dönüştürmek ve ihlalde davranışı (SIGSYS/EPERM/audit) göstermek. Araç bir
**sertleştirme** aracıdır; saldırı amacı taşımaz, tek başına izolasyon (namespace/cgroup) yerine geçmez.

## 2. İşletim sistemi arka planı (F2)
- **System call arayüzü (H1):** user mod `syscall` talimatıyla çekirdeğe geçer; çağrı numarası `rax`'tadır,
  argümanlar `rdi, rsi, rdx, r10, r8, r9`. Dönüş `rax`'tadır; hata `-errno` olarak döner.
- **seccomp-bpf (H14):** `seccomp(2)`/`prctl(PR_SET_SECCOMP)` ile süreç başına filtre; her syscall girişinde
  çekirdek `struct seccomp_data` üzerinde klasik BPF çalıştırır. Filtreler fork/clone ile **miras** kalır,
  `execve` ile **sürer**, geri alınamaz; çoklu filtrelerde en kısıtlayıcı sonuç kazanır.
- **`no_new_privs`:** root olmayan süreç yalnızca bu bit açıkken filtre yükleyebilir; ayrıca `execve` ile
  ayrıcalık yükselmesini (setuid) kapatır. Sıra: `prctl(NO_NEW_PRIVS)` → `seccomp_load` → `execve`
  (test T17 `NoNewPrivs: 1`, `Seccomp: 2` gösterir).
- **Eylemler:** `KILL_PROCESS` (SIGSYS, çekirdek audit `type=1326`), `ERRNO`, `LOG`, `TRACE` (ptrace
  olayı), `USER_NOTIF`. Projede ilk dördü kullanıldı (`kill`, `errno`, `log`, `say`=TRACE).
- **ptrace ile syscall durdurma:** SAY modunda `PTRACE_O_TRACESECCOMP` stop'ta `orig_rax` okunur;
  `orig_rax=-1`, `rax=-EPERM` yazılarak çağrı atlanır ve EPERM taklit edilir.
- Filtrenin BPF içeriği ve mimari/x32 kontrolü: `docs/04-mimari.md`.

## 3. Yöntem
Akış ve bileşenler `docs/04-mimari.md`'de. Ölçüm yöntemi `docs/05-olcum-yontemi.md`'de.

## 4. Sonuçlar (kanıtlı)
| Gereksinim | Kanıt |
|---|---|
| `make` sıfır uyarı | `olcum/2026-11-01-derleme.txt` → `0` |
| `make test` | 5 betik, 23 test grubu, 89 kontrol yeşil (`olcum/2026-11-01-test.txt`) |
| ASan/UBSan | `make asan` → `asan/ubsan: temiz` (`-asan.txt`); kapsam yalnızca profil/filtre/hata testleri; SAY ptrace döngüsü ve sandbox altı testler sanitizer'sız |
| Profil deterministik | iki üretim: `.seccomp` sha256 `6ac7efd2…eab1`, `.bpf` `be5302bd…de25` (her ikisi aynı) |
| Kapatılan syscall | 368 toplam, 45 izinli, **323 kapalı (%87,8)** |
| Profilsiz/profilli çıktı aynı | T10 (stdout karşılaştırması) |
| SIGSYS (`olcum/2026-11-01-audit.txt`) | exit 159, `strace`: `+++ killed by SIGSYS +++`, `journalctl -k`: `type=1326 ... sig=31 syscall=272` |
| EPERM | `ret=-1 errno=1 (Operation not permitted)`, süreç yaşar |
| Engellenen sayısı/adı | SAY: `engellenen system call sayisi: 1`, `unshare (nr=272) x1` |
| 5 tekrarlı ölçüm | `olcum/2026-11-01-olcum.csv`, `-ozet.md`, `-sure.png` |

Ölçüm tablosu (ort ± sd, n=5): bkz. `rapor/k2-ilk-olcum.md` §4.

## 5. Bulgular
1. **`strace -c` güvenilir izin listesi vermez.** Özet tablo hiç dönmeyen çağrıları (`exit`, `exit_group`) saymaz;
   bu tablodan üretilen profille hedef ilk thread çıkışında SIGSYS aldı. Tam `strace -f` izi ayrıştırıldığında
   sorun yok (T3 sınırı belgeler).
2. **Boş/eksik profil + ERRNO**, çocuğun `exit_group` çağıramayıp SIGSEGV ile çökmesine yol açtı (T20 yakaladı);
   sandbox hata yolu için `exit_group`'u ekler ve bunu stderr'e yazar.
3. **SAY modu eksik profille askıda kalabilir** (thread `exit` engellenince ptrace EPERM taklidi thread'in
   çıkmasını engeller); testler `timeout` ile korunur.
4. **Yerel ayar (tr_TR)** strace tablosunda ondalık virgül üretir; `LC_ALL=C` zorunlu.
5. **Filtre ek yükü ~10 ms'lik koşuda gürültüden ayırt edilemedi** (10,04 ± 0,50 vs 10,25 ± 0,41 ms).
6. `dmesg` bu sistemde root ister (`dmesg_restrict=1`); `journalctl -k` root'suz audit satırını gösterdi.

## 6. Kırılma testleri (F5) — DURUM: YAPILMADI
Tasarım `docs/03-risk-ve-b-plan.md`'de; **henüz hiçbiri çalıştırılmadı.** Final öncesi yapılacak:
`kill -9` (sandbox/çocuk), disk dolu (güvenli yol: `ulimit -f` ile `EFBIG`/SIGXFSZ, gerçek diski doldurmadan),
`systemd-run --user -p MemoryMax=64M`, `-p CPUQuota=20%`, izinsiz syscall + audit (**bu son madde yapıldı**: §4).

## 7. Tekrar üretilebilirlik
`scripts/kur.sh` / `scripts/kur-rootsuz.sh`, `make surum`, sabit tohum gerekmez (program deterministik),
`TEKRAR`/`BIN` ortam değişkenleri, `Dockerfile` + `docker-compose.yml` + `scripts/test.sh` + CI iş akışı.
Docker koşusu (temiz `ubuntu:24.04`, sistem libseccomp 2.5.5): `docker compose run --rm test` → `scripts/test.sh: HEPSI YESIL`, çıkış 0 (`olcum/2026-11-01-docker.txt`). Bu koşu ilk denemede T15'te **başarısız** oldu: konteynerde strace `killed by SIGSYS (core dumped)` yazıyor; test eşleşmesi gevşetildi.

## 8. Sınırlar
Dinamik profil kapsamı senaryolara bağlıdır; x86_64'e özgü; sanitizer'lı hedef sandbox altında
çalıştırılmaz; ölçümler tek makinede alındı; seccomp tek başına yeterli izolasyon değildir.

## 9. Tamamlanmayanlar (dürüst liste)
- [ ] Kırılma testleri (§6)
- [ ] Demo videosu çekimi (senaryo hazır)
- [ ] Temiz ortamda ölçüm tekrarı ve K2 güncellemesi
- [ ] `v0.5` / `v1.0` etiketleri (ilgili tarihlerde)
