# 01 — Heilmeier Soruları (8 soru)

> Sayılar bu depodaki komutların çıktısıdır (bkz. `rapor/k2-ilk-olcum.md`); ölçümler
> 01.11.2026 tarihli ön ölçümlerdir, K2'de temiz ortamda tekrarlanacaktır.

## 1. Ne yapmaya çalışıyorsunuz? (jargonsuz)
Bir programın işletim sisteminden isteyebileceği şeyleri (system call) yalnızca **gerçekten
ihtiyaç duyduğu** isteklerle sınırlamak. Programı normal çalışırken izleyip hangi istekleri
yaptığını **otomatik** çıkarıyoruz; sonra programı, listede olmayan her isteği reddeden (ya da
programı öldüren) bir kafeste çalıştırıyoruz. Program ele geçirilse bile listede olmayan bir şeyi —
yeni program başlatmak, mount etmek, namespace açmak gibi — yapamaz.

## 2. Bugün nasıl yapılıyor, sınırları neler?
- **Elle yazılmış genel profiller:** Docker'ın varsayılan seccomp profili, systemd
  `SystemCallFilter=@system-service`. Geniş ve geneldir; belirli bir programa gereğinden fazla izin verir
  ve glibc güncellemeleriyle (`clone3`, `rseq`) sessizce eskiyebilir.
- **OpenBSD `pledge()`:** programcı kaynak koda yazar; Linux'ta karşılığı yok.
- **Dinamik izleme araçları** (`strace -c` tabanlı betikler): kaç çağrı yapıldığını söyler ama
  **kendi başına güvenilir bir izin listesi vermez.** Bu projede bunu somut gördük: `strace -c`
  özet tablosu hiç dönmeyen çağrıları (`exit_group`, thread `exit`) **saymıyor**; bu tablodan üretilen
  profille hedef program ilk thread çıkışında SIGSYS ile ölüyor (bkz. `rapor/final-rapor.md` §Bulgular).
- **Temel sınır:** dinamik profil yalnızca çalıştırılan yolları görür; test edilmemiş bir hata yolu
  profilde yoksa üretimde süreç öldürülür.

## 3. Yaklaşımınızda yeni olan ne, neden başarılı olacağını düşünüyorsunuz?
- İzin listesi elle değil, `strace -f` izinden **thread'ler dahil** çıkarılır; birden çok senaryo
  (normal / hata / socket) birleştirilir.
- Çıktı **deterministik**: aynı girdi → bayt bayt aynı `.seccomp` ve `.bpf` (sha256 ile doğrulanır).
- Profilin **kapattığı** çağrı sayısı ölçülür: libseccomp'a göre x86_64'te 368 çağrıdan 45'i izinli,
  **323'ü kapalı (%87,8)**.
- İhlal dört modda gösterilir: KILL (SIGSYS), ERRNO (EPERM), LOG (kernel audit) ve **SAY**
  (`SCMP_ACT_TRACE` + ptrace ile engellenen çağrıları sayıp adlandıran, root gerektirmeyen mod).
- Neden başarılı oldu: libseccomp olgun, çekirdek desteği makinede doğrulandı; kapsam dar ve her
  davranış testle sabitlendi (23 test grubu, 89 kontrol).

## 4. Kimin umurunda? Başarılı olursa ne fark eder?
- Servis/daemon yazanlar için dakikalar içinde programa özel dar profil.
- Konteyner kullanıcıları için Docker'ın geniş profili yerine programa özel profil.
- Ders açısından user/kernel sınırı, system call tablosu, BPF, sinyaller ve audit tek araçta birleşir (H1 + H14).
- Fark ölçülebilir: erişilebilir çekirdek yüzeyi 368 → 45 çağrı.

## 5. Riskler neler?
Ayrıntı: [03-risk-ve-b-plan.md](03-risk-ve-b-plan.md). Gerçekleşenler: `strace -c` eksikliği (çözüldü),
`libseccomp-dev` kurulumu için sudo yokluğu (rootsuz yedekle çözüldü), SAY modunda eksik profil
yüzünden askıda kalma (belgelendi). Açık: kapsama eksikliği, glibc/çekirdek sürüm bağımlılığı.

## 6. Ne kadar maliyetli?
Donanım/lisans maliyeti yok (açık kaynak araçlar, mevcut dizüstü). Emek: yaklaşık 6–8 saat/hafta.

## 7. Ne kadar sürer?
Dönem boyunca ~10 hafta; çekirdek akış ilk gün çalışır hale geldi, K2–K3 aralığı sağlamlaştırma,
temiz ortam doğrulaması ve video içindir. Ayrıntı: [02-haftalik-plan.md](02-haftalik-plan.md).

## 8. Ara ve final kontrollerde başarı nasıl ölçülecek?
| Kontrol | Ölçüt | Kanıt |
|---|---|---|
| K1 | Açık depo, belgeler | GitHub bağlantısı |
| K2 | Hedef profilsiz/profilli çalışır, 5 tekrar ort ± sd | `olcum/*.csv`, `rapor/k2-ilk-olcum.md` |
| K3 | Akış, iki ihlal modu, ≥10 test yeşil, `v0.5`, ≤3 dk video | `make test`, `demo/` |
| Final | Deterministik filtre, temiz ortamda yeşil test, kırılma testleri | `scripts/test.sh`, `docker compose run --rm test` |

Başarı eşikleri (hepsi testle doğrulanıyor): profilli çalışmada stdout profilsizle aynı; yasaklı çağrıda
KILL → SIGSYS (exit 159), ERRNO → EPERM; aynı girdi → aynı sha256.
