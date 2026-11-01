# seccomp ile System Call İzin Listesi

**BMU1301 İşletim Sistemleri · 2026–2027 Güz · Proje #018 · Grup G · Tema: Güvenlik · Zorluk: z3**

Bir programın gerçekten kullandığı system call'ları **çalışırken otomatik çıkarıp** yalnızca onlara
izin veren bir seccomp-bpf sandbox'ı. Amaç saldırı değil, **sertleştirme**: bir süreç ele geçirilse
bile çekirdeğe açılan kapıyı daraltmak.

> Durum: **Faz 0 / K1 hazırlığı.** Henüz çalışan araç yok; aşağıdaki "5 dakikada çalıştır"
> bölümü Faz 2'de gerçek komutlarla doldurulacak ve test edilecek.

## Büküm (projeye özgü katkı)
1. İzin listesi elle yazılmaz; `strace -f` / ptrace ile hedef programın çalışmasından çıkarılır.
2. Profilin **kapattığı** syscall sayısı ölçülür (mimarideki toplam − izinli).
3. İzinsiz bir çağrıda süreç nasıl sonlanır (SIGSYS / EPERM) ve günlükte nasıl görünür
   (`audit: type=1326 ... syscall=N`) gösterilir.

## Mimari (özet)
```
hedef ──strace -f──▶ profil (JSON/MD) ──libseccomp──▶ profiller/hedef.seccomp (BPF)
                                                              │
                         sandbox --mod kill|errno ────────────┘──▶ execve(hedef)
```
Ayrıntı: [docs/04-mimari.md](docs/04-mimari.md) (Faz 1'de yazılacak).

## Gereksinimler
- Linux ≥ 5.x (x86_64), Ubuntu 24.04 veya WSL2
- gcc, make, pkg-config, **libseccomp-dev**, strace, python3-matplotlib

```bash
sudo scripts/kur.sh      # yalnızca paket kurulumu root ister
make surum               # ortam bilgisini yazdırır (raporlara eklenir)
make                     # -Wall -Wextra -Werror ile derler
```

## 5 dakikada çalıştır
_Faz 2'de eklenecek._

## Belgeler
| Dosya | İçerik |
|---|---|
| [docs/00-proje-kunyesi.md](docs/00-proje-kunyesi.md) | Künye, takvim, puanlama |
| [docs/01-heilmeier.md](docs/01-heilmeier.md) | Heilmeier soruları |
| [docs/02-haftalik-plan.md](docs/02-haftalik-plan.md) | Haftalık plan |
| [docs/03-risk-ve-b-plan.md](docs/03-risk-ve-b-plan.md) | Risk tablosu ve B planı |

## Lisans
MIT — bkz. [LICENSE](LICENSE).
