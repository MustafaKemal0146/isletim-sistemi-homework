# Final Raporu (çalışma taslağı)

> Durum: 01.11.2026 (v0.2). Çekirdek, ölçümler ve kırılma testleri bu tarihte; **yazılı olmayan** (yapılmamış) işler §9'da
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
| `make test` | 7 betik, 28 test grubu, 117 kontrol yeşil (`olcum/2026-11-01-test.txt`) |
| ASan/UBSan | `make asan` → `asan/ubsan: temiz` (`-asan.txt`); kapsam yalnızca profil/filtre/hata testleri; SAY ptrace döngüsü ve sandbox altı testler sanitizer'sız |
| Profil deterministik | iki üretim: `.seccomp` sha256 `6ac7efd2…eab1`, `.bpf` `8de743bf…bcd0` (her ikisi aynı; 52 talimat) |
| Kapatılan syscall | 368 toplam, 45 izinli, **323 kapalı (%87,8)** |
| Profilsiz/profilli çıktı aynı | T10 (stdout karşılaştırması) |
| SIGSYS (`olcum/2026-11-01-audit.txt`) | exit 159, `strace`: `+++ killed by SIGSYS +++`, `journalctl -k`: `type=1326 ... sig=31 syscall=272` |
| Mimari atlatma (x32, `int 0x80`) | `bin/arch_testi`: 5/5 yol yakalandı, çağrı hiçbirinde çalışmadı, SIGSYS handler'ı çalışmadı (`olcum/2026-11-01-calistir.txt`) |
| Profil başlığı doğrulama | `arch`/`surum`/`libseccomp` uyuşmazlığı → çıkış 5 (T24) |
| RSS | 3 bağımsız yöntemle doğrulandı (`olcum/2026-11-01-rss.txt`) |
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
5. **Filtre ek yükü ~10 ms'lik koşuda gürültüden ayırt edilemedi** (10,46 ± 1,34 vs 10,70 ± 0,62 ms).
6. **Yanlış-mimari eylemi KILL_THREAD idi** (libseccomp varsayılanı); `ACT_BADARCH=KILL_PROCESS` ile sertleştirildi (docs/04).
7. **`kill -9` ile sandbox ölünce komut yetim kalıyordu**; `PR_SET_PDEATHSIG` ile çözüldü (önce/sonra: `olcum/kirilma-kill9.txt`).
8. `ru_maxrss`'in fork mirasıyla bozulduğu varsayımım **yanlıştı**; ölçüm tersini gösterdi (docs/05).
9. Kendi hatam: `hedef disk` güvenlik sınırında `4096u*1024u*1024u` 32 bit taşması vardı (sınır 0 oluyordu), ilk disk testi bu yüzden diski doldurmadı; fark edilip düzeltildi, test yeniden koşuldu.
10. `dmesg` bu sistemde root ister (`dmesg_restrict=1`); `journalctl -k` root'suz audit satırını gösterdi.

## 6. Kırılma testleri (F5) — DURUM: yapıldı (01.11.2026)
Komutlar `tools/kirilma.sh` içinde (`make kirilma`); her biri çıktısını `olcum/kirilma-<ad>.txt` dosyasına yazar.
Kök gerektirmez; gerçek disk doldurulmadı.

| Test | Önce / koşul | Sonra / davranış | Kanıt |
|---|---|---|---|
| **kill -9 hedefe** | `hedef uzun 30` çalışıyor | sandbox `sinyal 9 (Killed)` raporlar, **çıkış 137**, zombi 0, kalan süreç 0; ardından `hedef tam` çıkış 0 | `kirilma-kill9.txt` A |
| **kill -9 sandbox'a** | **önce:** çocuk yetim kaldı (PPID 1421'e geçti, çalışmaya devam etti) | **sonra:** `PR_SET_PDEATHSIG` ile çocuk da sonlanır; SAY modunda `EXITKILL` ile aynı | `kirilma-kill9.txt` B/C, T26 |
| **Disk dolu** (20 MB tmpfs, userns) | `df`: 20M, 0 kullanılan | `write` → ENOSPC: `hedef: disk dolu: write basarisiz: No space left on device (errno=28), yazilan=20480 KB`, geçici dosya silindi, **çıkış 3 (tanımlı hata, sinyal yok)**, çökme yok; yer açılınca tekrar çalışır | `kirilma-disk.txt` |
| **MemoryMax=32M**, `bellek 16` | cgroup `memory.peak=18845696` | çıkış 0, `oom_kill 0` | `kirilma-bellek.txt` A |
| **MemoryMax=32M**, `bellek 64` | 24 MiB'a kadar ilerledi | cgroup OOM: sandbox `sinyal 9 (Killed)`, **çıkış 137**, `memory.events: oom_kill 1`; çekirdek günlüğü: `hedef invoked oom-killer ... CONSTRAINT_MEMCG`; sandbox ayakta kalıp raporladı; sonra tekrar çalışır | `kirilma-bellek.txt` B/C |
| **CPUQuota=20%**, `hesap 300M`, 5 tekrar | sınırsız: **400,6 ± 4,9 ms** | kota: **1963,9 ± 40,3 ms** (yavaşlama **×4,90**, beklenen ≈5); sonuç her 10 koşuda aynı `79dea8608c645e2d` | `kirilma-cpu.txt`, `kirilma-cpu.csv` |

Notlar: (1) SIGKILL'in `kill -9` mi OOM mı olduğunu sandbox ayırt edemez (ikisi de sinyal 9); ayrım
cgroup `memory.events` ve çekirdek günlüğündendir. (2) Kırılma betiğinin süreç sayımı kendi UID'iyle sınırlıdır: eşzamanlı çalışan Docker testinin `hedef`'i ilk denemede "kalan süreç: 1" yanlış alarmı verdi, düzeltildi ve testler temiz koşuda tekrarlandı. (3) `hesap` syscall yapmaz; CPU kotası altında doğruluk
korunduğunu gösterir, seccomp ek yükünü değil. (4) Disk testi `unshare -Urm` gerektirir (kullanıcı
namespace'i kapalı sistemlerde çalışmaz; o durumda çıktıda `unshare cikis` ≠ 0 görünür, test başarılı sayılmaz).

## 7. Tekrar üretilebilirlik
`scripts/kur.sh` / `scripts/kur-rootsuz.sh`, `make surum`, sabit tohum gerekmez (program deterministik),
`TEKRAR`/`BIN` ortam değişkenleri, `Dockerfile` + `docker-compose.yml` + `scripts/test.sh` + CI iş akışı.
Docker koşusu (temiz `ubuntu:24.04`, sistem libseccomp 2.5.5): `docker compose run --rm test` → `scripts/test.sh: HEPSI YESIL`, çıkış 0 (`olcum/2026-11-01-docker.txt`, v0.2 koduyla yeniden koşuldu). Bu koşu ilk denemede T15'te **başarısız** oldu: konteynerde strace `killed by SIGSYS (core dumped)` yazıyor; test eşleşmesi gevşetildi.

## 8. Sınırlar
Dinamik profil kapsamı senaryolara bağlıdır; x86_64'e özgü; sanitizer'lı hedef sandbox altında
çalıştırılmaz; ölçümler tek makinede alındı; seccomp tek başına yeterli izolasyon değildir.

## 9. Tamamlanmayanlar (dürüst liste)
- [ ] Demo videosu çekimi (senaryo hazır)
- [ ] Temiz ortamda ölçüm tekrarı ve K2 güncellemesi
- [ ] `v0.5` / `v1.0` etiketleri (ilgili tarihlerde)
