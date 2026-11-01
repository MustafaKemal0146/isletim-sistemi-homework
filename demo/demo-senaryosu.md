# Demo Senaryosu (≤ 3 dakika) — son hâl

Kaydı sen alacaksın; bu belge çekime hazır akıştır. Tüm komutlar depoda gerçekten çalıştırıldı ve
çıktıları doğrulandı (aşağıdaki "Beklenen çıktı" sütunu gerçek çıktıdan).

## Çekim öncesi (kayıt dışı, bir kez)
```bash
cd ~/isletim-sistemi-homework
make && bash scripts/calistir.sh >/dev/null 2>&1      # bin/ ve profiller/ hazır olsun
clear
```
Terminal: koyu tema, yazı tipi ≥ 18 pt, 100×30. `dmesg` root ister, **ekranda çalıştırma** (journalctl kullan).
Önerilen: komutları önceden bir dosyaya yazıp `bash -x`'siz tek tek yapıştır; her komuttan sonra 1–2 sn bekle.

## Akış

| Süre | Söylenecek (kısa) | Komut | Beklenen çıktı (gerçek) |
|---|---|---|---|
| 0:00–0:15 | "Bir programın kullandığı system call'ları otomatik çıkarıp yalnızca onlara izin veriyorum: seccomp-bpf." | `cat README.md \| sed -n 8,14p` | büküm maddeleri |
| 0:15–0:40 | "Hedef program dosya, bellek, thread, socket kullanıyor. strace ile üç senaryoda izleyip birleştiriyorum." | `bin/profil -n 2 -o /tmp/p.json -- bin/hedef normal` | `profil: 35 farkli syscall -> /tmp/p.json` |
| 0:40–1:00 | "368 system call'dan yalnızca 45'ine izin var, 323'ü kapalı. Aynı girdiden hep aynı filtre." | `bin/profil_uret profiller/hedef.json -o /tmp/a.seccomp --ozet 2>/dev/null; sha256sum /tmp/a.seccomp profiller/hedef.seccomp` | `toplam_x86_64=368 izinli=45 kapatilan=323 oran=87.8%`, iki aynı sha256 (`6ac7efd2…`) |
| 1:00–1:15 | "Profille çalışınca çıktı aynı." | `bin/sandbox --mod kill --profil profiller/hedef.seccomp -- bin/hedef tam 2>&1 \| tail -3` | `hedef: normal/hata/socket tamam` satırları |
| 1:15–1:45 | **"Şimdi canlı ihlal: hedef, listede olmayan `unshare` çağrısını yapıyor."** | `bin/sandbox --mod kill --profil profiller/hedef.seccomp -- bin/hedef ihlal; echo "cikis=$?"` | `cocuk sinyal ile sonlandi: 31 (Bad system call)`, `cikis=159` |
| 1:45–2:05 | "Kanıt: strace ve çekirdek audit kaydı." | `strace -f -o /tmp/s.txt bin/sandbox --mod kill --profil profiller/hedef.seccomp -- bin/hedef ihlal >/dev/null 2>&1; grep -B1 'killed by SIGSYS' /tmp/s.txt \| cut -c1-90; journalctl -k --since '-1 min' --no-pager \| grep type=1326 \| tail -1 \| cut -c1-230` | `unshare(0) = 272`, `+++ killed by SIGSYS +++`, `audit: type=1326 ... sig=31 ... syscall=272` |
| 2:05–2:25 | "Aynı çağrı ERRNO modunda: EPERM, süreç yaşıyor. SAY modu engellenen çağrıyı sayıp adlandırıyor." | `bin/sandbox --mod errno --profil profiller/hedef.seccomp -- bin/hedef ihlal 2>&1 \| tail -1; bin/sandbox --mod say --profil profiller/hedef.seccomp -- bin/hedef ihlal 2>&1 \| tail -2` | `ret=-1 errno=1 (Operation not permitted)`; `engellenen system call sayisi: 1`, `engellendi: unshare` |
| 2:25–2:40 | "Mimari atlatma: izinli `write`'ı x32 ve i386 yoluyla denemek de yakalanıyor." | `bin/arch_testi --profil profiller/hedef.seccomp \| tail -4` | `... YAKALANDI` satırları, `genel: BASARILI` |
| 2:40–2:55 | "Kırılma testi: çalışırken `kill -9`; sandbox raporlıyor, zombi yok, yetim yok." | `bash tools/kirilma.sh kill9 2>&1 \| sed -n '/### A)/,/### A.)/p' \| head -20` | `sandbox cikis kodu: 137`, `zombi surec sayisi : 0` |
| 2:55–3:00 | "Testler yeşil; kırılma kayıtları olcum/ içinde." | `make test 2>&1 \| tail -1` | `test tamam: 7 betik` |

> Süre sıkışırsa atla: 0:00–0:15 satırı ve x32 satırı; "kill -9" satırını 2:40'a çek. 3 dakikayı geçmemek için
> her komut çıktısını `tail`/`head` ile kısalttık; çıktı akıyorsa `Ctrl-L` ile temizle.

## Çekilmesi önerilen ek (opsiyonel, 3 dk dışı)
- `bash tools/kirilma.sh bellek` (OOM, `memory.events: oom_kill 1`) ve `kirilma.sh cpu` (×4,9 yavaşlama).
  Süre uzun olduğu için ana videoya konmadı; çıktıları `olcum/kirilma-*.txt` içinde.

## Yedek planlar
- `journalctl -k` satırı çıkmazsa (WSL2/başka makine): strace satırı `+++ killed by SIGSYS +++` yeterli kanıttır.
- `tools/kirilma.sh kill9` takılırsa: `pkill -KILL -x hedef`.
- Savunma sorusu: "SIGSYS'i yakalayıp devam edemez mi?" → `KILL_PROCESS` yakalanamaz; `arch_testi` x32+handler satırı
  (`handler_calisti=hayir`) gösterir.
