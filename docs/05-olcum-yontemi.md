# 05 — Ölçüm Yöntemi

Komut: `make && bash scripts/calistir.sh && TEKRAR=5 bash tools/olcum.sh` (yalnızca `tools/olcum.sh`
profil dosyası `profiller/hedef.seccomp` ve `profiller/hedef.json` var olmalıdır).

## Senaryolar
| Ad | Mod | Hedef argümanı | Anlam |
|---|---|---|---|
| `yok-tam` | filtre yok | `tam` | taban çizgisi |
| `kill-tam` | KILL | `tam` | filtreli, izinli akış |
| `errno-tam` | ERRNO | `tam` | filtreli, izinli akış |
| `say-tam` | SAY | `tam` | ptrace'li sayım, ihlal yok |
| `say-ihlal` | SAY | `ihlal` | 1 engellenen çağrı sayılır |
| `kill-ihlal` | KILL | `ihlal` | SIGSYS, çıkış 159 |

## Metrikler (CSV: `tekrar,senaryo,syscall_sayisi,engellenen,sure_ms,rss_kb,exit_kodu`)
- `sure_ms`: sandbox'ın `fork` öncesinden çocuğun bitişine `CLOCK_MONOTONIC` farkı (fork+exec dahil).
- `rss_kb`: `wait4` rusage `ru_maxrss` (zirve RSS, KB). **Bağımsız doğrulama** (`tools/rss_dogrula.sh`,
  çıktı `olcum/2026-11-01-rss.txt`): hedefin kendi başına `/usr/bin/time -v` değeri 10608–10736 KB,
  sandbox üzerinden değer 10608–10736 KB, sandbox'ın kendi ayak izi 1256 KB (`/bin/true` ile), cgroup
  `memory.peak` 10,5–11,2 MB (sayfa önbelleği dahil, bayt cinsinden yük). Yani sütun hedefin zirve RSS'ini
  doğru ölçüyor; fork mirası `max()` ile alındığı için (1256 KB ≪ 10,6 MB) sonucu bozmuyor. (Önceki sürümde burada
  "miras kalan yüksek su çizgisi sütunu bozar" yazıyordu; bu bir tahmindi ve ölçümle çürütüldü.) Sütunun
  senaryolar arasında neredeyse sabit olması beklenen sonuçtur: hedef her senaryoda ~8 MiB ayırır, filtre
  belleği değiştirmez.
- `engellenen`: SAY'da ptrace ile sayılan çağrılar; KILL'de SIGSYS ile ölmüşse 1 (ilk ihlalde ölür,
  daha fazlası görülemez); `yok`'ta 0.
- `syscall_sayisi`: aynı senaryo argümanıyla **filtresiz** hedefin `strace -f` izindeki çağrı sayısı
  (her tekrarda ayrı koşu). KILL ihlalinde hedef son çağrıyı yaptıktan sonra ölür; filtreli koşuda gerçek
  sayı bu değerden birkaç çağrı azdır.
- `exit_kodu`: sinyalle ölümde 128+N.

## Prosedür
1 ısınma koşusu atılır (disk önbelleği, dinamik bağlayıcı); sonra 5 tekrar. Ortalama ve **örnek**
standart sapma (n−1), min–max hesaplanır (`awk`, `LC_ALL=C` — yerel ayar ondalık virgülü bozmasın).
Çıktılar: `olcum/<tarih>-olcum.csv`, `-ozet.md`, `-sure.png`. Ortam bilgisi (`uname`, gcc, libseccomp,
strace, CPU) özet dosyasının başına yazılır.

## Geçerlilik tehditleri
- Süreler ~10 ms: zamanlayıcı gürültüsü ve fork/exec maliyeti baskın; filtre ek yükü bu ölçekte
  gürültüden ayırt edilemiyor. Bu bir **bulgudur**, "ek yük sıfırdır" iddiası değil.
- Tek makine, tek çekirdek sürümü; temiz Ubuntu 24.04 VM/konteynerde tekrar K2'de yapılacak.
- n = 5 küçük bir örnektir; güven aralığı verilmiyor.
