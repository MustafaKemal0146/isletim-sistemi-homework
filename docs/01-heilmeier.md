# 01 — Heilmeier Soruları (taslak)

> Taslak. Sayısal hedefler K2 ölçümlerinden sonra gerçek değerlerle güncellenecek;
> şu an yazan sayılar **hedef**tir, ölçüm değildir.

## 1. Ne yapmaya çalışıyorsunuz? (jargonsuz)
Bir programın işletim sisteminden isteyebileceği şeyleri, yalnızca **gerçekten ihtiyaç duyduğu**
isteklerle sınırlamak. Program normal çalışırken hangi istekleri (system call) yaptığını kendimiz
otomatik olarak kaydediyoruz; sonra programı, bu listenin dışındaki her isteği reddeden bir
kafeste çalıştırıyoruz. Program saldırgan tarafından ele geçirilse bile, listede olmayan bir şeyi
— örneğin yeni bir program başlatmayı veya ağ bağlantısı açmayı — yapamıyor.

## 2. Bugün nasıl yapılıyor, sınırları neler?
- **Elle yazılmış profiller:** Docker'ın varsayılan seccomp profili (~300+ çağrıya izin verir),
  systemd'nin `SystemCallFilter=@system-service` grupları, Chrome/Firefox'un el yapımı
  sandbox politikaları. Geniş ve geneldir; belirli bir program için gereğinden fazla izin verir.
  Elle bakımı zordur, glibc güncellemesiyle (ör. `clone3`, `rseq`) sessizce bozulabilir.
- **OpenBSD `pledge()`:** Programcı kaynak koduna yazar; Linux'ta yok, kod değişikliği ister.
- **Otomatik araçlar** (ör. Podman `oci-seccomp-bpf-hook`, akademik çalışmalar) vardır ama
  ölçüm şeffaflığı ve "neyi kapattık?" sorusunun sayısal cevabı genelde verilmez.
- **Temel sınır:** Dinamik (çalışma zamanı) profil yalnızca **çalıştırılan yolları** görür;
  test edilmemiş bir hata yolu profilde yoksa üretimde program öldürülür.

## 3. Yaklaşımınızda yeni olan ne, neden başarılı olacağını düşünüyorsunuz?
- İzin listesi **elle değil**, `strace -f` ile çalışma sırasında, thread'ler dahil çıkarılır.
- Çıktı **deterministik**: aynı profil → bayt bayt aynı BPF (`sha256` ile doğrulanır).
- Profilin **kapattığı** çağrı sayısı açıkça ölçülür: `x86_64 toplam syscall − izinli`.
- İhlal iki modda gösterilir: `KILL_PROCESS` (SIGSYS, audit kaydı `type=1326`) ve
  `ERRNO(EPERM)` (süreç yaşar, çağrı başarısız olur) — davranış farkı ölçülür.
- Neden başarılı olur: libseccomp olgun bir kütüphane; çekirdek desteği (seccomp-bpf, 3.5+)
  mevcut makinede doğrulandı (`actions_avail` bütün eylemleri içeriyor). Risk düşük, kapsam net.

## 4. Kimin umurunda? Başarılı olursa ne fark eder?
- Sunucu/servis yazanlar: kendi daemon'ları için dakikalar içinde dar bir profil üretebilir.
- Konteyner/CI kullanıcıları: Docker'ın geniş varsayılan profili yerine programa özel profil.
- Ders açısından: user/kernel sınırı, system call tablosu, BPF, sinyaller ve audit alt sistemi
  tek bir somut araçta birleşir (H1 + H14).
- Fark ölçülebilir: hedef programın erişebildiği çekirdek yüzeyi ~370 çağrıdan **hedef: <60**
  çağrıya iner (≈ %85 daralma — K2'de gerçek değer ölçülecek).

## 5. Riskler neler?
Ayrıntı [03-risk-ve-b-plan.md](03-risk-ve-b-plan.md). En önemlileri:
kapsama eksikliği (çalışmayan kod yolu profilde yok), audit günlüğüne root'suz erişim
(`dmesg_restrict=1`), KILL modunda yalnızca **ilk** ihlalin görülmesi, ASan'ın ek syscall'ları,
glibc/çekirdek sürümüne bağlı profil farkları.

## 6. Ne kadar sürer, maliyeti nedir?
- Süre: ~9 hafta (02.11.2026 → final). Plan: [02-haftalik-plan.md](02-haftalik-plan.md).
- Maliyet: Donanım/lisans maliyeti yok; açık kaynak araçlar ve mevcut dizüstü/VM.
  Tahmini emek ≈ 6–8 saat/hafta.

## 7. Başarıyı ara ve final sınavlarda nasıl ölçeceksiniz?
| Kontrol | Ölçüt | Kanıt |
|---|---|---|
| K1 | Açık depo, bu belgeler, ilk commit | GitHub bağlantısı |
| K2 | Hedef profilsiz/profilli çalışır, 5 tekrar ortalama ± sapma | `olcum/*.csv`, `rapor/k2-ilk-olcum.md` |
| K3 | Profil→filtre→sandbox akışı, iki ihlal modu, ≥8 test yeşil, `v0.5` | `make test` çıktısı, video |
| Final | Deterministik filtre, Docker'da yeşil test, kırılma testleri, final rapor | `scripts/test.sh`, demo |

Başarı eşikleri:
- Profilli çalışmada hedef programın çıktısı profilsizle **aynı** (exit 0, aynı stdout özeti).
- Yasaklı çağrıda KILL modunda süreç **SIGSYS (31)** ile ölür; ERRNO modunda çağrı `EPERM` döner.
- Aynı girdiden iki kez üretilen `hedef.seccomp` dosyalarının `sha256` değeri aynı.
- Filtre ek yükü raporlanır (ölçülen değer; şimdilik bir eşik iddia edilmiyor).
