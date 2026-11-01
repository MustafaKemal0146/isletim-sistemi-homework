# Demo Senaryosu (≤ 3 dakika)

Terminal: büyük yazı tipi, `cd ~/isletim-sistemi-homework`. Tüm komutlar depoda gerçekten çalışır;
konuşma metni kısa tutuldu. **Video henüz çekilmedi** (K3 işi).

| Süre | Ekran | Söylenecek | Komut |
|---|---|---|---|
| 0:00–0:15 | başlık | "seccomp-bpf ile bir programın izin listesini otomatik çıkarıp sınırlıyorum." | — |
| 0:15–0:40 | profil | "Hedef program dosya, bellek, thread ve socket kullanıyor. strace ile çalışırken izliyorum." | `bash scripts/calistir.sh 2>&1 \| head -14` |
| 0:40–1:00 | kapanan yüzey | "368 çağrının 45'i izinli, 323'ü kapalı." | `cat profiller/hedef.seccomp \| head -8` |
| 1:00–1:15 | determinizm | "Aynı girdi, aynı sha256." | `bin/profil_uret profiller/hedef.json -o /tmp/a.seccomp 2>/dev/null; sha256sum /tmp/a.seccomp profiller/hedef.seccomp` |
| 1:15–1:35 | izinli çalışma | "Profille hedef normal çalışıyor, çıktı aynı." | `bin/sandbox --mod kill --profil profiller/hedef.seccomp -- bin/hedef tam` |
| 1:35–2:05 | **canlı ihlal: KILL** | "Hedef profilde olmayan unshare'i çağırıyor: SIGSYS, çıkış 159." | `bin/sandbox --mod kill --profil profiller/hedef.seccomp -- bin/hedef ihlal; echo "cikis=$?"` |
| 2:05–2:25 | kanıt | "Çekirdek günlüğünde de görünüyor: type=1326, syscall=272." | `bash scripts/audit_kaniti.sh \| sed -n '/== 2/,/== 5/p'` |
| 2:25–2:45 | ERRNO ve SAY | "Aynı çağrı bu kez EPERM; süreç yaşıyor ve engellenen sayısı raporlanıyor." | `bin/sandbox --mod say --profil profiller/hedef.seccomp -- bin/hedef ihlal` |
| 2:45–3:00 | kapanış | "Testler yeşil, ölçümler olcum/ içinde." | `make test 2>&1 \| tail -3` |

## Çekim notları
- Hazırlık: `make && bash scripts/calistir.sh >/dev/null 2>&1` (profil önceden hazır olsun).
- `dmesg` root ister; kanıt olarak `journalctl -k` ve strace kullanılıyor (ekranda gösterme).
- Yedek plan: journalctl satırı çıkmazsa (WSL2) strace satırı `+++ killed by SIGSYS +++` yeterli.
