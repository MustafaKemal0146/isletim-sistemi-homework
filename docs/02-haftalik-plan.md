# 02 — Haftalık Plan (taslak)

Takvim haftaları 02.11.2026'dan başlar. Her hafta en az bir anlamlı commit hedeflenir
(resmi kural: commit geçmişi ≥ 5 farklı haftaya yayılmalı). Final tarihi duyurulunca
W9–W10 güncellenecek.

| Hafta | Tarihler | Faz | İş | Çıktı / Kabul | Etiket |
|---|---|---|---|---|---|
| W1 | 02–08 Kas | 0–1 | Repo iskeleti, Makefile, Heilmeier, plan, risk; `libseccomp-dev` kurulumu; mimari taslak (strace vs ptrace kararı) | `docs/00–04`, `make kontrol` yeşil | — |
| W2 | 09–15 Kas | 1→2 | **K1 teslimi (12.11).** `src/hedef.c` (dosya, socket, mmap, zaman, thread); `tools/profil` prototipi (strace -f çıktısını ayrıştırma → JSON/MD) | Hedef profilsiz çalışır; profil JSON üretilir | `v0.1` |
| W3 | 16–22 Kas | 2 | `tools/profil_uret.c` (libseccomp, sıralı ekleme, `seccomp_export_bpf`); `src/sandbox.c` (`PR_SET_NO_NEW_PRIVS` → `seccomp_load` → `execve`), `--mod kill|errno` | Hedef profille çalışır; aynı girdi → aynı sha256 | — |
| W4 | 23–29 Kas | 2 | `tools/olcum.sh` (5 tekrar, ort ± sapma, süre, RSS, syscall sayısı); `tools/sayac.c` (kapatılan çağrı sayısı); **K2 teslimi (26.11)** | `olcum/2026-11-2x-*.csv`, `rapor/k2-ilk-olcum.md` | `v0.2` |
| W5 | 30 Kas–06 Ara | 3 | İhlal senaryoları (hedef `--ihlal` bayrağı), engellenen çağrıların sayımı (ERRNO+strace, SCMP_ACT_LOG); hata yolları; `test/*.sh` ≥ 8 test; ASan/UBSan | `make test` yeşil, `make sanitize` temiz | — |
| W6 | 07–13 Ara | 3 | Demo senaryosu + ≤3 dk video; ara rapor; **K3 teslimi (10.12)** | `demo/demo-senaryosu.md`, `rapor/k3-ara-rapor.md` | `v0.5` |
| W7 | 14–20 Ara | 4 | Kırılma testleri: kill -9, disk dolu, MemoryMax=64M, CPUQuota=20%, izinsiz syscall + audit | Her biri için komut + çıktı kaydı | — |
| W8 | 21–27 Ara | 4 | Docker/`scripts/test.sh` temiz ortam; sürüm sabitleme; matplotlib grafikleri | `docker compose run --rm test` yeşil | `v0.8` |
| W9 | 28 Ara–03 Oca | 4 | Final raporu (BPF açıklaması, ölçümler, sınırlar); reviewer denetimi ve düzeltmeler | `rapor/final-rapor.md` | `v1.0-rc` |
| W10 | Final haftası | 4 | Savunma provası (iki soru + Git geçmişi), son temizlik | Demo provası | `v1.0` |

## Ders haftalarıyla bağ
- **H1 (system call arayüzü):** `syscall` talimatı, `rax`'ta syscall numarası, user→kernel
  geçişi; strace'in ptrace ile bu geçişleri nasıl yakaladığı → profil aracı.
- **H14 (güvenlik):** en az ayrıcalık ilkesi, seccomp-bpf, `no_new_privs`, audit → sandbox ve
  ihlal gösterimi.

## Haftalık rutin
1. Pazartesi: haftanın hedefini bu tabloya göre `docs/02` içinde işaretle.
2. Her iş parçası ayrı commit (`feat:`, `fix:`, `test:`, `docs:`).
3. Pazar: `make test` + kısa ilerleme notu (rapor taslağına).
