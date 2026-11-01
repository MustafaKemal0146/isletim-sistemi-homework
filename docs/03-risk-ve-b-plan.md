# 03 — Risk Tablosu ve B Planı (taslak)

Olasılık (O) ve Etki (E): 1 düşük – 3 yüksek. Skor = O × E.

| # | Risk | O | E | Skor | Erken uyarı | Önlem | B planı |
|---|---|---|---|---|---|---|---|
| R1 | **Kapsama eksikliği:** dinamik profil yalnızca çalışan yolları görür; hata yolundaki bir çağrı (ör. `write(2,…)` hata mesajı) profilde yok → üretimde süreç öldürülür | 3 | 3 | 9 | Profilli çalışmada beklenmedik SIGSYS | Hedefi birden çok senaryoyla (normal + hata yolları) çalıştırıp profilleri birleştir; önce `SCMP_ACT_LOG` ile "öğrenme modu" | Sandbox varsayılanı KILL'dir; ERRNO modu 'yumuşak' alternatiftir (öğrenme için LOG). Eksik çağrı listesi raporda açıkça belirtilir |
| R2 | **Audit günlüğüne root'suz erişim yok:** `kernel.dmesg_restrict=1` (bu makinede doğrulandı) | 3 | 2 | 6 | `dmesg` → "Operation not permitted" | `journalctl -k` (adm grubu), yoksa root adımı olarak ayrıca belgelenir | Root gerektirmeyen kanıt: `strace -f` çıktısındaki `+++ killed by SIGSYS +++` ve `si_syscall` alanı; wait status'tan sinyal no |
| R3 | **KILL modunda yalnızca ilk ihlal görülür** → "engellenen sayısı" eksik | 3 | 2 | 6 | Sayım her zaman 1 | Sayım için ERRNO modu + strace `-e status=failed` veya `SCMP_ACT_LOG`; KILL yalnızca sonlanma gösterimi | `SCMP_ACT_TRACE` + ptrace ile kendi sayacımız (`tools/sayac.c`) |
| R4 | **`libseccomp-dev` kurulu değil** (bu makinede doğrulandı; sudo parola istiyor) | 3 | 3 | 9 | `make kontrol` hatası | `sudo scripts/kur.sh` W1'de çalıştırılır | Docker imajında derleme (`ubuntu:24.04` + paket) |
| R5 | **glibc/çekirdek sürümüne bağlı profil:** `clone3`, `rseq`, `newfstatat` vb. sürüme göre değişir; başka makinede profil kırılır | 2 | 3 | 6 | Docker'da test kırmızı | Profil, çalışacağı ortamda üretilir; `make surum` çıktısı `olcum/*-ozet.md` ortam bölümüne yazılır (profil başlığına yazma yapılmadı) | Sürüm uyuşmazlığında sandbox uyarı verip reddeder |
| R6 | **ASan/UBSan ek syscall kullanır** (`mmap`, `sigaltstack`, `readlink` /proc…) → sanitizer'lı hedef, normal profille çalışmaz | 3 | 1 | 3 | ASan derlemesinde SIGSYS | Sanitizer koşusu profilsiz yapılır ya da ayrı profil üretilir; raporda açıklanır | Sanitizer'ı yalnızca araçlara (profil, üretici, sandbox) uygula |
| R7 | **Ortam farkı:** geliştirme Zorin OS 18 (Ubuntu 24.04 tabanlı); WSL2'de audit alt sistemi farklı olabilir | 2 | 2 | 4 | WSL2'de `type=1326` satırı yok | Resmi ölçümler Ubuntu 24.04 konteyner/VM'de | WSL2 için strace kanıtına geç (R2 B planı) |
| R8 | **Ölçüm gürültüsü:** süre ölçümleri μs-ms mertebesinde, CPU frekansı/ısınma etkili | 2 | 2 | 4 | Sapma > ortalamanın %20'si | 1 ısınma koşusu atılır, 5 tekrar, `taskset`; ortam sabitleri CSV başlığına | Tekrar sayısını artır (raporda belirtilerek) |
| R9 | **Mimari/ABI kaçışı:** x86_64 filtre i386/x32 ABI çağrılarını (`int 0x80`) kapsamazsa atlatma | 1 | 3 | 3 | — | libseccomp'un varsayılan arch kontrolü (yerel olmayan arch → `KILL`) açık bırakılır; test eklendi: T23 (`tools/arch_testi.c`) | Raporda sınır olarak belirt |
| R10 | **Docker içinde seccomp/ptrace:** Docker'ın kendi profili iç içe filtreye ve strace'e izin vermeyebilir | 2 | 2 | 4 | Konteynerde `ptrace: Operation not permitted` | `--cap-add SYS_PTRACE` veya `security_opt: seccomp=unconfined` yalnızca test servisinde | `scripts/test.sh` ile VM'de temiz koşu |
| R11 | **Zaman:** dönem sonu yoğunluğu, final tarihi belirsiz | 2 | 3 | 6 | Haftalık hedef iki hafta üst üste kaçarsa | K3'e kadar çekirdek akış bitmiş olmalı; Docker/grafik sonra | Kapsam daraltma sırası: grafik → Docker → SCMP_ACT_TRACE sayacı |

## Gerçekleşen riskler ve durum (01.11.2026)
| # | Durum | Kanıt |
|---|---|---|
| R2 | **Hafifledi:** bu makinede `dmesg` root ister, ama `journalctl -k` root'suz `type=1326` satırını gösterdi | `scripts/audit_kaniti.sh` çıktısı, `rapor/final-rapor.md` |
| R4 | **Çözüldü (yedekle):** sudo yok; `scripts/kur-rootsuz.sh` ile `apt-get download` + `dpkg-deb -x` → `.deps` (statik bağlama). `sudo scripts/kur.sh` yine önerilen yol | `make` yeşil |
| R1 | **Gerçekleşti ve çözüldü:** `strace -c` tablosu `exit`/`exit_group`'u saymıyor, profille hedef ilk thread çıkışında SIGSYS aldı. Çözüm: tam iz ayrıştırma | docs/04, test T3 |
| R12 (yeni) | SAY modunda profilde `exit` eksikse ptrace EPERM verir ve thread hiç çıkamaz → askıda kalma. Önlem: SAY testleri `timeout 30` ile koşar, belgelendi | docs/04 |
| R13 (yeni) | Yerel ayar (tr_TR) ondalık virgülü strace tablosunu ve awk'ı bozdu. Önlem: `LC_ALL=C` | `tools/profil.c`, `tools/olcum.sh` |
| R15 (yeni) | `kill -9` ile sandbox ölürse komut yetim kalıyordu → `PR_SET_PDEATHSIG` ile çözüldü; önce/sonra kaydı `olcum/kirilma-kill9.txt` |
| R16 (yeni) | Yanlış-mimari varsayılanı KILL_THREAD idi → `ACT_BADARCH=KILL_PROCESS`, test T23 |
| R14 (yeni) | Boş profil + ERRNO: çocuk `exit_group`'u çağıramayıp SIGSEGV'e düştü. Önlem: sandbox hata yolu için `exit_group` ekler | test T20 |

## Kapsam daraltma sırası (B planı özeti)
Zorunlu ve **asla kesilmeyecek**: otomatik profil çıkarma, filtre üretimi, sandbox, iki ihlal
modu, kapatılan syscall sayısı, 5 tekrarlı ölçüm, ≥10 test.
Gerekirse kesilecekler (sırasıyla): matplotlib grafikleri → Docker compose (yerine
`scripts/test.sh`) → SAY modu (yerine ERRNO + strace sayımı).

## Kırılma testleri (F5): tasarım ve gerçekleşen sonuç
Sonuçlar: `olcum/kirilma-{kill9,disk,bellek,cpu}.txt`, özet `rapor/final-rapor.md` §6. Aşağıdaki tablo ilk tasarımdı.
| Saldırı | Beklenen davranış | Kanıt |
|---|---|---|
| `kill -9` | Sandbox çocuğun wait status'unu raporlar (SIGKILL=9), yarım CSV satırı yazılmaz | log satırı + exit kodu |
| Disk dolu (`fallocate`) | `write` → `ENOSPC`, anlamlı hata, exit `CIKIS_SISTEM` | stderr + exit kodu |
| `MemoryMax=64M` | OOM'da süreç SIGKILL; sandbox bunu ayırt edip raporlar | `systemd-run` çıktısı |
| `CPUQuota=20%` | Sonuç doğru, süre ~5× uzar; ölçüm tablosunda gösterilir | CSV |
| İzinsiz syscall | SIGSYS (31) veya EPERM; audit `type=1326 syscall=N` | strace/journal satırı |
