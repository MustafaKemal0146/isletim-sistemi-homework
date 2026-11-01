# 04 — Mimari

## Akış
```
                 ┌──── 3 senaryo × N kosu ────┐
bin/hedef ──strace -f──▶ bin/profil ──▶ profiller/hedef.json (birlesim, sirali)
                                            │
                                  bin/profil_uret (libseccomp)
                                            ▼
                    profiller/hedef.seccomp (metin)   profiller/hedef.bpf (52 talimat)
                                            │
        bin/sandbox --mod kill|errno|log|say|yok --profil ... -- komut
                  fork ─▶ [cocuk] prctl(NO_NEW_PRIVS) ─▶ seccomp_load ─▶ execve(komut)
                       └▶ [ebeveyn] wait4 (+ptrace SAY modunda) ─▶ rapor, olcum satiri
```

## Bileşenler
| Dosya | Görev |
|---|---|
| `src/hedef.c` | Çok çeşitli syscall yapan örnek program (dosya, bellek, zaman, thread, socket, hata yolu); `ihlal` senaryosu hiçbir profilde olmayan `unshare(0)` çağırır |
| `src/sandbox.c` | Filtreyi kurar ve yükler, komutu çalıştırır, sonucu raporlar |
| `tools/profil.c` | strace izinden JSON profil; birleştirme |
| `tools/profil_uret.c` | JSON → `.seccomp` + `.bpf`; kapatılan çağrı sayısı |
| `tools/ihlal_testi.c` | İzinsiz çağrıda KILL ve ERRNO davranışını kanıtlar |
| `tools/olcum.sh`, `tools/grafik.py` | 5 tekrarlı ölçüm, CSV, özet, PNG |
| `src/json_mini.h`, `src/profil_oku.h` | Kütüphanesiz katı JSON okuyucu; `.seccomp` okuyucu ve filtre kurucu |

## Profil çıkarma: neden `strace -f` izi, `strace -c` tablosu değil?
İlk uygulama `strace -f -c` özet tablosunu ayrıştırıyordu. Hedef program profille çalıştırılınca
**SIGSYS ile öldü**. `strace -f` izinde ölen thread'in son çağrısı `exit(0)` idi; `-c` tablosunda ne
`exit` ne `exit_group` vardı: **hiç dönmeyen çağrılar özet tabloya girmiyor.** Bu yüzden canlı profil
tam izden (`strace -f -qq -s 0 -o dosya`) ayrıştırılır; `-c` tablosu yalnızca golden testte kullanılır
(`--strace-girdi`) ve bu sınır bir testle sabitlenmiştir (T3).

**strace mi ptrace mı?** Kendi ptrace izleyicimizi yazmak yerine strace seçildi: olgun, thread/fork
takibi hazır, çıktısı insan tarafından da denetlenebilir. Ptrace yalnızca SAY modunda (engellenen çağrıları
saymak için) kullanılır.

## Filtre üretimi ve determinizm
- Adlar alfabetik (`strcmp`) sıralanır, tekrarlar atılır, zaman damgası yazılmaz.
- Bilinmeyen ad **hata**dır (sessizce atlanmaz) → çıkış kodu 5.
- Varsayılan eylem `SCMP_ACT_KILL_PROCESS`; her izinli çağrı için `SCMP_ACT_ALLOW` kuralı.
- Doğrulama: iki üretim `sha256sum` ile karşılaştırılır (T5, T6): `.seccomp` ve `.bpf` ikisi de aynı.

## BPF nasıl çalışıyor? (gerçek `profiller/hedef.bpf`'den)
Çekirdek her syscall girişinde, süreçteki filtreyi `struct seccomp_data {nr, arch, instruction_pointer, args[6]}`
üzerinde çalıştırır; program klasik BPF'tir (8 baytlık talimat: `code, jt, jf, k`). Üretilen 416 baytlık
program 52 talimattır (Python ile çözülmüş gerçek çıktı):
```
00: ld  [4]            ; A = seccomp_data.arch
01: jeq #0xc000003e    ; x86_64 degilse 51'e atla (jf=49) -> KILL_PROCESS
02: ld  [0]            ; A = seccomp_data.nr
03: jge #0x40000000    ; nr >= 0x40000000 (x32 ABI araligi) ise 04'e, degilse 05'e atla (jf=1)
04: jeq #0xffffffff    ; nr == -1 ise 05'e dus (hicbir kiyas eslesmez -> 51); degilse 51'e atla (jf=46)
05: jeq #0   -> jt=44  ; nr == read(0)   ise 50'ye (ALLOW)
06: jeq #1   -> jt=43  ; nr == write(1)  ise 50'ye (ALLOW)
07: jeq #3 ...         ; (sirali nr karsilastirmalari, 45 adet; son: 0x1b3 = clone3)
49: jeq #0x1b3 jt=0 jf=1
50: ret 0x7fff0000     ; SECCOMP_RET_ALLOW
51: ret 0x80000000     ; SECCOMP_RET_KILL_PROCESS (varsayilan eylem VE yanlis mimari)
```
Yani: **önce mimari doğrulanır** (i386 `int 0x80` ve x32 ile filtre atlatma engellenir), sonra çağrı numarası
izin listesiyle karşılaştırılır; eşleşme yoksa varsayılan eylem döner. Eylem kodunun üst 16 biti türü,
alt 16 biti verisini taşır (`SCMP_ACT_ERRNO(EPERM)` = `0x00050001`).

**Mimari atlatma testi ve bir sertleştirme (`tools/arch_testi.c`, test T23).** İzinli bir çağrıyı (`write`)
başka bir ABI üzerinden yapma denemeleri: `syscall(0x40000000 + SYS_write)` (x32) ve `int $0x80` (i386).
Filtre hepsini yakaladı (marker hiçbir yolda çıktıya ulaşmadı). Yine de ilk sürümde libseccomp'un yanlış-mimari
varsayılanı **`SECCOMP_RET_KILL_THREAD` (0)** idi; bu eylem process'i değil yalnızca thread'i öldürür ve
(çekirdek sürümüne bağlı olarak) SIGSYS yakalanabilir. `filtre_kur()` artık `SCMP_FLTATR_ACT_BADARCH =
KILL_PROCESS` ayarlar; bu yüzden yukarıdaki listede 52. talimat kaldırıldı (53 → 52) ve `.bpf` sha256 değişti
(`be5302bd…` → `8de743bf…`; `.seccomp` aynı kaldı). ERRNO modunda yanlış-mimari çağrı EPERM değil, süreç ölümüdür
(bilinçli: başka ABI kullanan bir hedef zaten şüphelidir).

## Yükleme sırası (sandbox)
1. `prctl(PR_SET_NO_NEW_PRIVS, 1)` — bunsuz root olmayan süreç filtre yükleyemez; ayrıca `execve` ile
   setuid ikili üzerinden ayrıcalık kazanılmasını engeller.
2. `seccomp_load()` — filtre tüm sonraki çağrılara (ve thread/fork çocuklarına) uygulanır.
3. `execvp()` — `execve` da profildedir (strace izi `execve` ile başlar). Test T17, hedefte
   `NoNewPrivs: 1` ve `Seccomp: 2` değerlerini gösterir.

`PR_SET_PDEATHSIG(SIGKILL)` çocukta ilk iş olarak ayarlanır: sandbox `kill -9` ile öldürülürse komut yetim
kalmaz (düzeltme öncesi kalıyordu; `olcum/kirilma-kill9.txt` önce/sonra). SAY modunda aynı garantiyi
`PTRACE_O_EXITKILL` verir.

Sandbox'ın kendi çocuğunun hata yolu için tek istisna: `exit_group` profilde yoksa eklenir ve bu
stderr'e yazılır (aksi halde boş profille ERRNO modunda çocuk çıkış yapamayıp SIGSEGV ile çöküyordu —
test T20 bunu yakaladı).

## İhlal modları
| Mod | Eylem | Gözlem |
|---|---|---|
| `kill` | `SCMP_ACT_KILL_PROCESS` | süreç SIGSYS(31) ile ölür; sandbox çıkış kodu 159; `journalctl -k`: `audit: type=1326 ... sig=31 syscall=272` |
| `errno` | `SCMP_ACT_ERRNO(EPERM)` | çağrı −1/EPERM döner, süreç yaşar |
| `log` | `SCMP_ACT_LOG` | çağrı çalışır, çekirdek audit'e yazar (profil çıkarma yardımcısı) |
| `say` | `SCMP_ACT_TRACE(1)` + ptrace | ptrace durağında çağrı numarası okunur, `orig_rax=-1`, `rax=-EPERM`; engellenen çağrılar sayılır ve adlandırılır |
| `yok` | filtre yok | ölçümde taban çizgisi |

Çıkış kodu politikası: komutun kendi kodu; sinyalle ölürse 128+N; sandbox hataları 2/3/4/5/127
(`src/ortak.h`).

## Hata modeli
Bozuk JSON, eksik dosya, bilinmeyen çağrı adı, bozuk `.seccomp`, geçersiz argüman → tanımlı çıkış
kodu ve anlamlı mesaj (test grubu T18–T21). `system()` hiçbir yerde kullanılmaz; alt süreçler
`fork`/`execvp` ile başlatılır ve durumları kontrol edilir.

## Sınırlar (dürüst liste)
- Dinamik profil yalnızca çalışan yolları kapsar (kapsama = senaryolar).
- x86_64'e özgü (SAY modu `user_regs_struct` kullanır).
- ASan/UBSan'lı hedef ek syscall kullanır; sandbox altında sanitizer'lı hedef çalıştırılmaz. `make asan` yalnızca profil/filtre/hata testlerini kapsar; SAY ptrace döngüsü sanitizer'sız.
- Profil JSON başlığı doğrulanır: `arch` ≠ `uname -m` veya libseccomp major.minor farkı → hata (çıkış 5); micro farkı kabul. `--uyusmazligi-yoksay` ile bilinçli geçilebilir.
- Kırılma testleri: `tools/kirilma.sh` (bkz. rapor/final-rapor.md §6).
- 368, libseccomp 2.5.5 tablosuna göredir (çekirdeğin tüm syscall kümesi değil).
- `ru_maxrss` bağımsız yöntemlerle doğrulandı (bkz. 05, `olcum/2026-11-01-rss.txt`).
- Bu bir **sertleştirme** aracıdır; seccomp tek başına izolasyon (namespace/cgroup) yerine geçmez.
