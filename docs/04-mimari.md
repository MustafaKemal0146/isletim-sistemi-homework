# 04 — Mimari

## Akış
```
                 ┌──── 3 senaryo × N kosu ────┐
bin/hedef ──strace -f──▶ bin/profil ──▶ profiller/hedef.json (birlesim, sirali)
                                            │
                                  bin/profil_uret (libseccomp)
                                            ▼
                    profiller/hedef.seccomp (metin)   profiller/hedef.bpf (53 talimat)
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
üzerinde çalıştırır; program klasik BPF'tir (8 baytlık talimat: `code, jt, jf, k`). Üretilen 424 baytlık
program 53 talimattır:
```
00: ld  [4]            ; A = seccomp_data.arch
01: jeq #0xc000003e    ; x86_64 degilse -> 52'ye atla (KILL)         (jf=50)
02: ld  [0]            ; A = seccomp_data.nr
03: jge #0x40000000    ; nr >= 0x40000000 (x32 ABI araligi) ise 04'e, degilse 05'e atla (jf=1)
04: jeq #0xffffffff    ; nr == -1 ise 05'e dus (hicbir kiyas eslesmez -> 50 KILL_PROCESS); degilse 52'ye atla (KILL_THREAD)
05: jeq #0   -> jt=45  ; nr == read(0)   ise 51'e (ALLOW)
06: jeq #1   -> jt=44  ; nr == write(1)  ise 51'e (ALLOW)
07: jeq #3 ... (sirali nr karsilastirmalari, 45 adet)
50: ret 0x80000000     ; SECCOMP_RET_KILL_PROCESS
51: ret 0x7fff0000     ; SECCOMP_RET_ALLOW
52: ret 0x00000000     ; SECCOMP_RET_KILL_THREAD (yanlis mimari)
```
Yani: **önce mimari doğrulanır** (i386/x32 `int 0x80` ile filtre atlatma engellenir), sonra çağrı numarası
izin listesiyle karşılaştırılır; eşleşme yoksa varsayılan eylem döner. Eylem kodunun üst 16 biti türü,
alt 16 biti verisini taşır (`SCMP_ACT_ERRNO(EPERM)` = `0x00050001`).

## Yükleme sırası (sandbox)
1. `prctl(PR_SET_NO_NEW_PRIVS, 1)` — bunsuz root olmayan süreç filtre yükleyemez; ayrıca `execve` ile
   setuid ikili üzerinden ayrıcalık kazanılmasını engeller.
2. `seccomp_load()` — filtre tüm sonraki çağrılara (ve thread/fork çocuklarına) uygulanır.
3. `execvp()` — `execve` da profildedir (strace izi `execve` ile başlar). Test T17, hedefte
   `NoNewPrivs: 1` ve `Seccomp: 2` değerlerini gösterir.

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
- x32/yanlış-mimari filtre atlatması için henüz test yok.
- `arch`/`surum` JSON alanları doğrulanmaz.
- 368, libseccomp 2.5.5 tablosuna göredir (çekirdeğin tüm syscall kümesi değil).
- `ru_maxrss` fork'tan miras kalan yüksek su çizgisini içerebilir (bkz. 05).
- Bu bir **sertleştirme** aracıdır; seccomp tek başına izolasyon (namespace/cgroup) yerine geçmez.
