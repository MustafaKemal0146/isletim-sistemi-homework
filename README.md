# seccomp ile System Call İzin Listesi

**BMU1301 İşletim Sistemleri · 2026–2027 Güz · Proje #018 · Grup G · Tema: Güvenlik · Zorluk: z3**
Mustafa Kemal Çıngıl · 24080410023 · Bilgisayar Mühendisliği 3. sınıf

Bir programın gerçekten kullandığı system call'ları **çalışırken otomatik çıkarıp** yalnızca onlara
izin veren bir seccomp-bpf sandbox'ı. Amaç saldırı değil, **sertleştirme**: bir süreç ele geçirilse
bile çekirdeğe açılan kapıyı daraltmak.

## Büküm (projeye özgü katkı)
1. İzin listesi elle yazılmaz; `strace -f` izinden, 3 senaryodan (normal/hata/socket) çıkarılır.
2. Profilin **kapattığı** syscall sayısı ölçülür: libseccomp 2.5.5'in x86_64 tablosundaki 368 çağrıdan **45 izinli, 323 kapalı (%87,8)**.
3. İzinsiz çağrıda süreç **SIGSYS** ile ölür (exit 159) veya **EPERM** alır; `strace`, `journalctl -k`
   (`audit: type=1326 ... sig=31 syscall=272`) ve SAY modu ile gösterilir.

```
hedef ──strace -f──▶ profil (JSON) ──libseccomp──▶ profiller/hedef.seccomp + .bpf
                                                         │
        sandbox --mod kill|errno|log|say ────────────────┘──▶ prctl(NO_NEW_PRIVS) → seccomp_load → execve(hedef)
```
Ayrıntı: [docs/04-mimari.md](docs/04-mimari.md).

## 5 dakikada çalıştır
Gereksinim: Ubuntu 24.04 (veya WSL2), `gcc make pkg-config strace`. **Root gerekmez** (root yalnızca isteğe bağlı paket kurulumu içindir).
```bash
git clone https://github.com/MustafaKemal0146/isletim-sistemi-homework && cd isletim-sistemi-homework
sudo scripts/kur.sh          # sudo yoksa:  scripts/kur-rootsuz.sh   (libseccomp-dev'i .deps'e indirir)
make                         # -Wall -Wextra -Werror, sıfır uyarı
make test                    # 23 test grubu (T1–T22, T9b), 89 kontrol
bash scripts/calistir.sh     # profil çıkar → filtre üret → KILL ile çalıştır → ihlal göster
```
Sonra:
```bash
bin/sandbox --mod kill  --profil profiller/hedef.seccomp -- bin/hedef ihlal; echo $?   # SIGSYS, 159
bin/sandbox --mod errno --profil profiller/hedef.seccomp -- bin/hedef ihlal            # EPERM, süreç yaşar
bin/sandbox --mod say   --profil profiller/hedef.seccomp -- bin/hedef ihlal            # engellenen sayısı + adı
bash scripts/audit_kaniti.sh                                                           # strace/journalctl/dmesg kanıtı
TEKRAR=5 bash tools/olcum.sh                                                           # 5 tekrarlı ölçüm → olcum/
make asan                    # ASan/UBSan (profil/filtre/hata testleri; ptrace döngüsü ve sandbox altı testler kapsam dışı)
bash scripts/test.sh         # temiz derleme + tüm testler + ASan (docker compose run --rm test aynısını yapar)
```

## Dizin yapısı
| Yol | İçerik |
|---|---|
| `src/hedef.c`, `src/sandbox.c` | örnek hedef, sandbox |
| `tools/profil.c`, `tools/profil_uret.c`, `tools/ihlal_testi.c` | profil çıkarma, filtre üretimi, ihlal kanıtı |
| `tools/olcum.sh`, `tools/grafik.py` | ölçüm ve grafik |
| `profiller/` | üretilen profil (`hedef.json`, `hedef.seccomp`, `hedef.bpf`) |
| `test/` | 5 betik, T1–T22, T9b |
| `olcum/` | CSV, özet, PNG |
| `docs/`, `rapor/`, `demo/` | belgeler, K1–final raporları, video senaryosu |
| `Dockerfile`, `docker-compose.yml`, `.github/workflows/ci.yml` | temiz ortam ve CI |

## Durum
Çekirdek akış, testler ve ön ölçümler tamam. K2'ye kalanlar (temiz ortamda ölçüm tekrarı), K3'e
kalanlar (video, `v0.5`) ve final işleri (kırılma testleri) `rapor/k3-ara-rapor.md` ve
`rapor/final-rapor.md` içinde dürüstçe listelenmiştir.

## Belgeler
[Künye](docs/00-proje-kunyesi.md) · [Heilmeier](docs/01-heilmeier.md) · [Plan](docs/02-haftalik-plan.md) ·
[Risk/B planı](docs/03-risk-ve-b-plan.md) · [Mimari](docs/04-mimari.md) · [Ölçüm yöntemi](docs/05-olcum-yontemi.md)

## Lisans
MIT — bkz. [LICENSE](LICENSE).
