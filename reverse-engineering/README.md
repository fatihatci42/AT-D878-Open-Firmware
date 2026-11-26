# Tersine Mühendislik (RE) Altyapısı

Bu klasör, AnyTone AT-D878UV/UVII V4.00 stok firmware'inin tersine mühendislik
araç zincirini içerir.

Stok firmware telif korumalıdır ve bu repoya dahil değildir; herkes kendi
kopyasını temin eder (`scripts/get_firmware.py` ile hash doğrulamalı).

## Bu Klasörün Görevi

### 1. Bilgi Entegrasyonu

`annotations/` altındaki tersine mühendislik bulguları (~700 fonksiyon adı,
~60 RAM global'i) Open Firmware geliştirmesine doğrudan referans olur:

- `annotations/names.txt` → Stok firmware'deki fonksiyon adresi → isim eşlemesi
- `annotations/globals.txt` → RAM adresi, tip, açıklama (OSRunning, screen_id, radio_settings…)
- `annotations/functions.txt` → Kesin fonksiyon sınırları
- `annotations/comments.txt` → Ghidra yorum notları
- `OVERVIEW.md` → Donanım, görevler, RF yolu, DMR, PC link için yoğun referans haritası

**Ne işe yarar?** `firmware/drivers/driver_at1846s.c` yazarken AT1846S'in
stok firmware'de bit-banged 3-wire GPIO (PB3/PB4/PB5) üzerinden erişildiğini
buradan öğrenirsiniz. `docs/HARDWARE_ANALYSIS.md`'deki "I2C mi SPI mi?" sorusu
burada yanıt bulur.

### 2. Araç Entegrasyonu

`tools/` altındaki Python araçları, radyo ile USB kablosu üzerinden doğrudan
iletişim kurar (debug patch yüklenmiş firmware gerektirir):

| Araç | İşlev | Open Firmware Karşılığı |
|------|-------|------------------------|
| `radio.py` | Flash/dump, peek/poke/call | `tools/radio_io.py` (çapraz platform) |
| `scope.py` | Spectrum waterfall terminalde | Studio LCD simülatörü |
| `txaudio.py` | USB'den WAV/TTS TX | Studio Audio modülü |
| `dec.py` / `decr.py` | Ghidra decompile | Studio Docs modülü |
| `trace.py` / `xrefs.py` | Fonksiyon analizi | — |
| `glob.py` | RAM global isimlendirme | — |

Windows'ta `tools/radio_io.py` kullanın (saf-pyserial, platform bağımsız).
Bu klasördeki `radio.py` Linux'a özgüdür (`fcntl`/`termios`).

## Kurulum

**Ön koşullar:** Linux, Python 3.10+, Ghidra 12.1.x, `clang`, `curl`.

```bash
export GHIDRA_INSTALL_DIR=~/opt/ghidra_12.1.4_PUBLIC

# Sanal ortam (capstone, keystone, unicorn, pyserial, pyghidra)
scripts/setup_venv.sh
. .venv/bin/activate

# Stok firmware'i hash doğrulamalı getir
scripts/get_firmware.py ~/Downloads/D878UV*.zip

# Referans datasheet'leri al (opsiyonel ama önerilir)
scripts/fetch_refs.sh

# Ghidra projesini kur (~4 dk)
scripts/bootstrap.sh

# Ghidra HTTP servisini başlat (127.0.0.1:8081)
scripts/ghidractl start

# Self-test
scripts/selftest.sh
```

## Günlük Kullanım

```bash
# AT1846S register 0x0D'yi yazan tüm fonksiyonları bul
tools/xrefs.py -w at1846_reg0d_shadow

# RF tuning fonksiyonunu decompile et
tools/dec.py 10 0x0805F504

# Yeni fonksiyon isimle
echo "0805F504 at1846_set_freq" | tools/ren.py
tools/names_snapshot.py

# Spectrum waterfall
tools/scope.py
```

## Klasör Bağımsızlığı

Bu klasördeki `tools/`, `docs/`, `scripts/` ve `firmware/` klasörleri
üst dizindeki Open Firmware projesinin aynı isimli klasörlerinden
**tamamen bağımsızdır** ve stok firmware RE altyapısına aittir.

Donanım bulgularının Open Firmware sürücülerine uygulanması için:
`../docs/REVERSE_ENGINEERING_BRIDGE.md`
