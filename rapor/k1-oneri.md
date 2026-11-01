# K1 — Proje Önerisi

**BMU1301 İşletim Sistemleri · Proje #018 · seccomp ile System Call İzin Listesi**
Mustafa Kemal Çıngıl · 24080410023 · Bilgisayar Mühendisliği 3. sınıf
Depo: https://github.com/MustafaKemal0146/isletim-sistemi-homework (açık)

## Özet
Bir programın kullandığı system call'ları çalışırken `strace -f` ile **otomatik** çıkarıp yalnızca
onlara izin veren bir seccomp-bpf sandbox'ı geliştireceğim (C11, libseccomp). Katalogdaki zorunlu
büküm üç parça: (1) izin listesi kendiliğinden çıkarılır, (2) profilin kapattığı syscall sayısı
sayılır, (3) izinsiz bir çağrıda programın nasıl sonlandığı ve günlükte nasıl göründüğü gösterilir.

## Belgeler
- Heilmeier soruları: [docs/01-heilmeier.md](../docs/01-heilmeier.md)
- Haftalık plan: [docs/02-haftalik-plan.md](../docs/02-haftalik-plan.md)
- Risk tablosu ve B planı: [docs/03-risk-ve-b-plan.md](../docs/03-risk-ve-b-plan.md)
- Mimari: [docs/04-mimari.md](../docs/04-mimari.md)

## Kapsam
Zorunlu: otomatik profil çıkarma, deterministik filtre üretimi, sandbox, KILL ve ERRNO modları,
kapatılan çağrı sayısı, 5 tekrarlı ölçüm, ≥ 10 test. Gerekirse kesilecekler (sırayla): grafikler,
Docker, SAY modu.

## Durum notu
Bu öneri belgesi, çekirdek akışın çalışır hale geldiği 01.11.2026 tarihinde yazıldı; K1 teslim
tarihi 12.11.2026'dır. Depo geçmişi gerçek commit tarihlerini taşır (geçmiş yeniden yazılmadı).
