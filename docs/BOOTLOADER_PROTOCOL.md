# AnyTone AT-D878 Bootloader ve USB Güncelleme Protokolü

**Hedef Donanım:** AnyTone AT-D878UVII Plus  
**Bağlantı Yöntemi:** USB Kablosu (Cihaz açılmadan, harici SWD gerektirmeden)

---

## 1. Giriş ve Güvenlik Felsefesi

Cihazın kasası açılmadan yapılacak tüm geliştirme ve yükleme süreçleri, cihazın
dahili ROM/Flash alanında bulunan **Üretici Bootloader'ı** üzerinden yürütülür.

### Temel Kural
> Orijinal bootloader asla silinmez veya üzerine yazılmaz.
> Uygulama yazılımı (Firmware) daima bootloader'ın kabul ettiği bellek adresine
> (`0x08004000`) yerleştirilir.

---

## 2. Bootloader Moduna Geçiş (Donanımsal Tetikleme)

AnyTone el telsizlerinde bootloader / firmware güncelleme moduna geçmek için
donanımsal tuş kombinasyonu kullanılır:

1. Telsiz **KAPALI** konuma getirilir.
2. **PTT** tuşu + **Üst Alarm/Turuncu Buton** (veya modele göre PTT + Yan Tuş 1) basılı tutulur.
3. Tuşlar basılıyken ses açma/kapama potu çevrilerek telsiz **AÇILIR**.
4. Cihaz ekranı kararır veya LED kırmızı/yeşil yanıp sönerek programlama modunu (DFU/Bootloader) gösterir.

---

## 3. AnyTone USB Seri İletişim Protokolü

USB kablosu takıldığında bilgisayar tarafında bir sanal seri port (Virtual COM Port) belirir.
Haberleşme varsayılan olarak **921600 baud** (veya bazı bootloader sürümlerinde **115200 baud**) hızında yürütülür.

### 3.1. Oturum Başlatma (Handshake)

```
Host (PC)                                   Radio (AnyTone)
   │                                               │
   │────────── "PROGRAM" (0x50 0x52...) ──────────►│
   │                                               │
   │◄───────── ACK (0x06) + Donanım ID ────────────│
   │                                               │
   │────────── Sürüm Sorgusu (0x02 ...) ──────────►│
   │                                               │
   │◄───────── "D878UVII" + Sürüm Bilgisi ─────────│
```

### 3.2. Çerçeve (Frame) Yapısı

Telsiz ile PC arasındaki paketler şu formata sahiptir:

| Alan | Boyut | Açıklama |
|------|-------|----------|
| **STX** | 1 Byte | Başlangıç belirteci (Genellikle `0x02` veya komut kodu) |
| **CMD** | 1 Byte | Komut kodu (Oku, Yaz, Sil, Kimlik Al) |
| **ADDR** | 3-4 Byte | Hedef Flash / Bellek adresi (Big-endian) |
| **LEN** | 2 Byte | Veri yükü (payload) uzunluğu |
| **DATA** | N Byte | Gerçek veri (Codeplug veya firmware bloğu) |
| **CHKSUM**| 1-2 Byte | Aritmetik veya CRC checksum |

---

## 4. Orijinal Firmware Güncelleme Dosyaları (.spi Analizi)

AnyTone resmi firmware güncellemelerini `.spi` uzantılı paketler halinde dağıtır.
Bu dosyalar incelendiğinde:

1. **Başlık Alanı:** Model uyumluluk imzası (`D878UVII`), sürüm numarası, paket boyutu ve CRC32 değeri.
2. **Şifreleme Durumu:** AnyTone firmware blokları basit bir XOR veya sabit tablo kaydırma (obfuscation) ile maskelenmiştir.
3. **Bellek Hedefi:** Çözümlenen kod blokları doğrudan MCU Flash adresine (`0x08004000`) yazılmak üzere düzenlenmiştir.

---

## 5. Güvenli Firmware Yükleme ve Geri Dönüş Rotası

```
┌────────────────────────────────────────────────────────┐
│           GELİŞTİRİLEN AÇIK KAYNAK FIRMWARE            │
│               (at_d878_fw.bin / .spi)                  │
└───────────────────────────┬────────────────────────────┘
                            │
                            ▼
┌────────────────────────────────────────────────────────┐
│                 USB GÜNCELLEME ARACI                   │
│         (tools/protocol_analyzer/anytone_protocol.py)  │
└───────────────────────────┬────────────────────────────┘
                            │ USB Serial
                            ▼
┌────────────────────────────────────────────────────────┐
│              CİHAZ ÜRETİCİ BOOTLOADER'I                 │
│                 (Adres: 0x08000000)                    │
├────────────────────────────────────────────────────────┤
│ - İmzayı doğrular                                      │
│ - Veriyi 0x08004000 adresine yazar                     │
│ - Hata olursa bootloader modunda kalır                 │
├────────────────────────────────────────────────────────┤
│                   UYGULAMA ALANI                       │
│                 (Adres: 0x08004000)                    │
└────────────────────────────────────────────────────────┘
```

> **Geri Dönüş Güvencesi:** Cihaz açılmadığı için üretici bootloader'ı silinemez.
> Açık kaynak firmware'den memnun kalınmaz veya bir hata oluşursa, cihaz tekrar
> tuş kombinasyonuyla açılıp orijinal V4.00 firmware dosyası CPS veya flash tool ile
> yüklenerek saniyeler içinde fabrika ayarlarına döndürülebilir.
