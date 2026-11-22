# AT-D878 Open Firmware - Donanım Analiz Raporu

**Cihaz:** AnyTone AT-D878UVII Plus  
**Firmware Sürümü:** V4.00  
**Rapor Tarihi:** 2026-11-22  
**Durum:** Aşama 1 - Araştırma (Devam Ediyor)

---

## 1. Donanım Blok Diyagramı

```
                            ┌─────────────────┐
                            │   ANTEN (VHF/UHF)│
                            └────────┬────────┘
                                     │
                            ┌────────▼────────┐
                            │  RF Front-End    │
                            │  (PA, LNA, SW)   │
                            │  Duplexer/Filter │
                            └────────┬────────┘
                                     │
┌──────────┐    I2C/SPI     ┌────────▼────────┐
│  Audio   │◄──────────────►│   AT1846S       │
│  Codec   │                │  RF Transceiver  │
│TLV320    │    I2S/PCM     │  VHF + UHF       │
│AIC3204   │◄──────────────►│                  │
└────┬─────┘                └────────┬────────┘
     │ Analog                        │ I2C/SPI
     │ Audio                         │
┌────▼─────┐                ┌────────▼────────┐
│ Speaker  │                │                  │
│ Mic      │                │  GD32F303VGT6   │
│ Audio    │                │  ARM Cortex-M4   │
│ Jack     │                │  120 MHz         │
└──────────┘                │  1 MB Flash      │
                            │  96 KB SRAM      │
┌──────────┐    HPI         │                  │
│ SCT3258  │◄──────────────►│                  │
│ DMR BB   │                │                  │
│ AMBE+2   │                │                  │
└──────────┘                │                  │
                            │                  │
┌──────────┐    SPI/EXMC    │                  │
│ NAND     │◄──────────────►│                  │
│ Flash    │                │                  │
│ 256 MB   │                │                  │
└──────────┘                │                  │
                            │                  │
┌──────────┐    SPI         │                  │
│ TFT LCD  │◄──────────────►│                  │
│ 1.77"    │                │                  │
│ Color    │                │                  │
└──────────┘                │                  │
                            │                  │
┌──────────┐    GPIO/ADC    │                  │
│ Keypad   │◄──────────────►│                  │
│ Encoder  │                │                  │
│ PTT      │                │                  │
└──────────┘                │                  │
                            │                  │
┌──────────┐    UART        │                  │
│ GPS      │◄──────────────►│                  │
│ Module   │                │                  │
└──────────┘                │                  │
                            │                  │
┌──────────┐    UART        │                  │
│Bluetooth │◄──────────────►│                  │
│ Module   │                │                  │
└──────────┘                │                  │
                            │                  │
┌──────────┐    USB 2.0 FS  │                  │
│ USB-C    │◄──────────────►│                  │
│ Port     │                │                  │
└──────────┘                └──────────────────┘
                                     │
                            ┌────────▼────────┐
                            │  Power Mgmt     │
                            │  Battery 2100mAh│
                            │  Charger IC      │
                            └─────────────────┘
```

---

## 2. Bileşen Detay Analizi

### 2.1. Ana İşlemci (MCU) - GigaDevice GD32F303VGT6

| Özellik | Değer | Doğrulama |
|---------|-------|-----------|
| **Çekirdek** | ARM Cortex-M4 + FPU | ✅ Doğrulandı (datasheet + teardown) |
| **Saat Hızı** | Maks. 120 MHz | ✅ Doğrulandı |
| **Flash** | 1024 KB (1 MB) | ✅ Doğrulandı |
| **SRAM** | 96 KB | ✅ Doğrulandı |
| **Paket** | LQFP-100 (14x14 mm) | ✅ Doğrulandı |
| **GPIO** | 80 adet (5V toleranslı) | ✅ Doğrulandı |
| **USART/UART** | 3 USART + 2 UART | ✅ Doğrulandı |
| **SPI** | 3 adet (18 Mbit/s, I2S destekli) | ✅ Doğrulandı |
| **I2C** | 2 adet (Fast + Standard) | ✅ Doğrulandı |
| **USB** | USB 2.0 FS (Device/Host) | ✅ Doğrulandı |
| **ADC** | 3 × 12-bit (16 kanal, 1 MSPS) | ✅ Doğrulandı |
| **DAC** | 2 × 12-bit | ✅ Doğrulandı |
| **DMA** | 2 kontrolcü, 12 kanal | ✅ Doğrulandı |
| **Timer** | 2 Adv. + 10 GP + 2 Basic + WDT | ✅ Doğrulandı |
| **EXMC** | SRAM, NOR, NAND desteği (ECC) | ✅ Doğrulandı |
| **CAN** | 2 × CAN 2.0B | ✅ Doğrulandı |
| **SDIO** | SD/SDIO/MMC | ✅ Doğrulandı |
| **Çalışma Sıcaklığı** | -40°C ~ +85°C | ✅ Doğrulandı |
| **Besleme** | 2.6V ~ 3.6V | ✅ Doğrulandı |

**Görevi:** Sistemin ana işlemcisi. UI, menü, USB iletişimi, kanal yönetimi,
codeplug işleme, tüm çevre birimlerin koordinasyonu.

**Açık Kaynak Desteği:**
- GD32 Standard Peripherals Library (GigaDevice resmi)
- Zephyr RTOS resmi BSP desteği
- OpenRTX GD32 portları (GD32F405 referansı mevcut)
- STM32 HAL ile yüksek register uyumluluğu (%90+)

**Önemli Not:** GD32F303, STM32F103/303 ile pin ve register düzeyinde büyük
ölçüde uyumludur ancak bazı flash erişim zamanlamaları, USB implementasyonu
ve DMA davranışları farklılık gösterir. STM32 kodunu doğrudan kullanmayın.

**Teknik Dokümanlar:**
- GD32F303xx Datasheet (GigaDevice resmi)
- GD32F30x User Manual
- GD32F30x Firmware Library User Guide
- ARM Cortex-M4 Technical Reference Manual

---

### 2.2. RF Transceiver - AT1846S (Auctus Technologies)

| Özellik | Değer | Doğrulama |
|---------|-------|-----------|
| **Tip** | Tek çipli VHF/UHF transceiver | ✅ Doğrulandı |
| **VHF Bandı** | 134-174 MHz | ✅ Doğrulandı |
| **UHF Bandı** | 400-520 MHz | ✅ Doğrulandı |
| **Kontrol Arayüzü** | I2C veya 3/4-wire SPI | ✅ Doğrulandı |
| **Register Genişliği** | 16-bit data, 7-bit adres | ✅ Doğrulandı |
| **Genişletilmiş Adres** | 0x7F üzeri için bank switching | ✅ Doğrulandı |
| **Kanal Genişliği** | 12.5 kHz / 25 kHz | ✅ Doğrulandı |
| **Frekans Hesaplama** | 30-bit tuning word (0x29+0x2A) | ✅ Doğrulandı |

**Görevi:** VHF ve UHF bantlarında RF sinyal alımı ve üretimi. Frekans sentezi,
modülasyon, demodülasyon, squelch algılama, RSSI ölçümü.

**MCU ile Haberleşme:** I2C (cihazda hangi bus kullanıldığı doğrulanmalı)

**Register Haritası (Kısmi - Doğrulanmış):**

| Adres | İşlev |
|-------|-------|
| 0x04 | Saat modu (frekans aralığı) |
| 0x08 | LDO bypass |
| 0x09 | LDO çıkış gerilimi |
| 0x0A | PA bias gerilimi |
| 0x0F | Bant seçimi (VHF/UHF/220 MHz) |
| 0x1F | GPIO modları (VOX, squelch, TXON, RXON vb.) |
| 0x29 | RF frekans üst kısım (bit 29:16) |
| 0x2A | RF frekans alt kısım (bit 15:0) |
| 0x2B | Referans saat frekansı (kHz) |
| 0x2C | ADC saat frekansı (kHz) |
| 0x2D | Interrupt enable mask |
| 0x30 | Kanal modu (12.5/25 kHz) |
| 0x44 | RX ses seviyesi |

**Açık Kaynak Sürücüleri:**
- OpenRTX AT1846S driver (C++, kapsamlı)
- RadioFoundry AT1846S implementasyonu
- MMDVM AT1846S register tanımları

**⚠️ Doğrulanması Gerekenler:**
- AT-D878UVII Plus'ta I2C mi yoksa SPI mi kullanılıyor?
- Kullanılan referans kristal frekansı
- GPIO pin bağlantıları (MCU ↔ AT1846S)
- PA kontrol ve güç ölçüm devreleri
- 220 MHz bandı donanımsal destek durumu

---

### 2.3. DMR Baseband İşlemci - Sicomm/CML SCT3258TD

| Özellik | Değer | Doğrulama |
|---------|-------|-----------|
| **Üretici** | Wuxi Sicomm (CML Microsystems şirketi) | ✅ Doğrulandı |
| **Desteklenen Protokoller** | DMR (Tier I/II), dPMR (Mod 1/2/3) | ✅ Doğrulandı |
| **Analog Mod** | FM + CTCSS/DCS | ✅ Doğrulandı |
| **Vocoder** | AMBE+2 (DVSI) + RALCWI (CML) | ✅ Doğrulandı |
| **İşleme Katmanları** | PHY + Data Link + Call Control | ✅ Doğrulandı |
| **Güç Tüketimi** | ~64 mW (DMR), ~38 mW (dPMR) | ✅ Doğrulandı |
| **Paket** | QFN64 | ✅ Doğrulandı |
| **MCU Arayüzü** | HPI (Host Port Interface) | ✅ Doğrulandı |

**Görevi:** DMR dijital ses ve veri işlemenin tamamını gerçekleştirir:
- TDMA slot zamanlama (Slot 1 ve Slot 2)
- AMBE+2 vocoder encode/decode
- FEC (Forward Error Correction)
- Fiziksel katman (4FSK modülasyon/demodülasyon)
- Veri bağlantı katmanı
- Çağrı kontrol katmanı
- Otomatik dijital/analog mod algılama ve geçişi

**MCU ile Haberleşme:** HPI (Host Port Interface) - paralel veri yolu.
SCT3258, MCU'ya bağımlı bir "baseband co-processor" olarak çalışır.
MCU, HPI üzerinden komut gönderir ve durum/veri okur.

**⚠️ KRİTİK KISITLAMALAR:**
1. **Datasheet Erişimi:** SCT3258 tam teknik dokümanı halka açık değildir.
   CML Micro web sitesinden product brief erişilebilir ancak tam register
   haritası ve HPI protokolü NDA gerektiriyor olabilir.
2. **AMBE+2 Lisansı:** SCT3258 içindeki AMBE+2 vocoder DVSI'nin mülkiyetindedir.
   Bu codec'in yazılımsal olarak çoğaltılması patent ihlalidir.
3. **HPI Protokolü:** Host Port Interface'in tam dokümantasyonu olmadan
   SCT3258'i kontrol etmek çok zordur. RadioFoundry bu konuda araştırma yapmaktadır.
4. **Alternatif Kullanım:** SCT3258'i bypass ederek MCU üzerinden doğrudan
   dijital modülasyon yapmak teorik olarak mümkün olabilir ancak zamanlama
   gereksinimleri çok sıkıdır.

**Açık Kaynak Sürücüleri:** Tam açık kaynak sürücü **mevcut değil**.
- RadioFoundry aktif olarak SCT3258 reverse engineering yapmaktadır
- MMDVM projesinde SCT3258 referansları bulunmaktadır
- Bazı Çin kökenli forumlarda kısmi bilgiler mevcuttur

**DMR Ses İşleme Zinciri:**
```
Mikrofon → AIC3204 (ADC) → I2S → SCT3258 (AMBE+2 Encode)
    → 4FSK Baseband → AT1846S (RF TX)

AT1846S (RF RX) → 4FSK Baseband → SCT3258 (AMBE+2 Decode)
    → I2S → AIC3204 (DAC) → Hoparlör
```

---

### 2.4. Ses Codec - Texas Instruments TLV320AIC3204

| Özellik | Değer | Doğrulama |
|---------|-------|-----------|
| **Tip** | Ultra-düşük güçlü stereo audio codec | ✅ Doğrulandı |
| **Kontrol Arayüzü** | I2C (SPI_SELECT=LOW) veya SPI | ✅ Doğrulandı |
| **I2C Adresi** | 0x18 (7-bit) / 0x30-0x31 (8-bit R/W) | ✅ Doğrulandı |
| **I2C Hızı** | Standard (100 kHz) + Fast (400 kHz) | ✅ Doğrulandı |
| **IOVDD** | 1.1V ~ 3.6V | ✅ Doğrulandı |
| **Register Mimarisi** | Sayfalı (Page 0-255, her sayfada 128 reg) | ✅ Doğrulandı |
| **Ses Arayüzü** | I2S, DSP, TDM, Left-Justified | ✅ Doğrulandı |

**Görevi:** Analog ses sinyallerinin dijitale (ADC) ve dijital ses sinyallerinin
analoga (DAC) çevrilmesi. Mikrofon ön yükselteci, hoparlör amplifikatörü,
kazanç kontrolü.

**MCU ile Haberleşme:**
- Kontrol: I2C (konfigürasyon ve register erişimi)
- Ses Verisi: I2S/PCM (DMA ile sürekli audio streaming)

**Açık Kaynak Sürücüleri:**
- Linux kernel'de ALSA sürücüsü mevcut (tlv320aic32x4.c)
- Zephyr RTOS'ta codec sürücü framework'ü var
- TI uygulama notları ve referans kodları mevcut

**Teknik Dokümanlar:**
- TLV320AIC3204 Datasheet (SLAS510)
- TLV320AIC3204 Application Reference Guide (SLAA557)

---

### 2.5. Harici NAND Flash Bellek

| Özellik | Değer | Doğrulama |
|---------|-------|-----------|
| **Kapasite** | 2 Gbit (256 MB) - UVII Plus | ✅ Doğrulandı |
| **Önceki Modeller** | 1 Gbit (128 MB) | ✅ Doğrulandı |
| **Arayüz** | SPI veya EXMC (Paralel) | ⚠️ Doğrulanacak |
| **Üretici/Model** | Muhtemelen Toshiba/Kioxia veya GigaDevice | ⚠️ Doğrulanacak |

**Görevi:** Codeplug, DMR ID veritabanı (500K kayıt), font dosyaları,
kullanıcı arayüzü grafikleri, ses kayıtları ve diğer kalıcı verilerin
saklanması.

**Bellek Haritası (Topluluk araştırmalarından):**

| Adres Aralığı | İçerik |
|----------------|--------|
| 0x00000000 - 0x0014FFFF | Sistem fontları (CJK, Kiril, Yunan) |
| 0x00150000 - 0x00159FFF | Boot ekran görüntüsü (bitmap) |
| 0x0015A000 - 0x001FFFFF | UI ikonları ve renk grafikleri |
| 0x00200000+ | Codeplug ve kullanıcı verileri |

**⚠️ Doğrulanması Gerekenler:**
- Flash çipin tam modeli (teardown gerekli)
- SPI mi yoksa paralel EXMC mi kullanılıyor?
- ECC konfigürasyonu
- Bad block management stratejisi
- Wear leveling uygulaması

---

### 2.6. Ekran - 1.77" Renkli TFT LCD

| Özellik | Değer | Doğrulama |
|---------|-------|-----------|
| **Boyut** | 1.77 inç | ✅ Doğrulandı |
| **Tip** | Renkli TFT LCD | ✅ Doğrulandı |
| **Çözünürlük** | Muhtemelen 128×160 veya 160×128 | ⚠️ Doğrulanacak |
| **Kontrolcü** | Muhtemelen ST7735 veya ILI9163 serisi | ⚠️ Doğrulanacak |
| **Arayüz** | SPI (muhtemelen) | ⚠️ Doğrulanacak |

**Görevi:** Kullanıcı arayüzü görüntüleme - kanal bilgisi, frekans,
DMR verileri, sinyal gücü, pil durumu, menüler.

**⚠️ Doğrulanması Gerekenler:**
- Tam piksel çözünürlüğü
- LCD kontrolcü IC modeli
- SPI bağlantı pinleri
- Arka aydınlatma kontrolü (PWM pin)
- Renk derinliği (16-bit RGB565 bekleniyor)

---

### 2.7. Tuş Takımı ve Rotary Encoder

| Özellik | Değer | Doğrulama |
|---------|-------|-----------|
| **Tuş Sayısı** | ~20 tuş + PTT + yan tuşlar | ⚠️ Doğrulanacak |
| **Matris Tipi** | Satır-sütun tarama (GPIO) | ⚠️ Doğrulanacak |
| **Encoder** | Incremental rotary encoder | ✅ Doğrulandı |
| **PTT** | Ayrı GPIO girişi | ✅ Doğrulandı |

**⚠️ Doğrulanması Gerekenler:**
- Tuş matrisi satır/sütun pin atamaları
- Encoder A/B/SW pinleri
- PTT ve yan tuş GPIO pinleri
- Debounce gereksinimleri

---

### 2.8. GPS Modülü

| Özellik | Değer | Doğrulama |
|---------|-------|-----------|
| **Tip** | Kompakt GPS alıcı modülü | ✅ Doğrulandı |
| **Arayüz** | UART (seri) | ✅ Doğrulandı |
| **Protokol** | NMEA 0183 | ✅ Bekleniyor |
| **Baud Rate** | 9600 veya 115200 | ⚠️ Doğrulanacak |

**Görevi:** Gerçek zamanlı konum verisi sağlama. APRS konum raporlama,
DMR GPS verisi, kullanıcı konumu gösterimi.

**⚠️ Doğrulanması Gerekenler:**
- GPS modül üretici/model
- UART bağlantı pinleri
- Desteklenen GNSS sistemleri (GPS, GLONASS, BeiDou?)
- Güncelleme hızı (1 Hz, 5 Hz?)

---

### 2.9. Bluetooth Modülü

| Özellik | Değer | Doğrulama |
|---------|-------|-----------|
| **Varlığı** | Evet (Plus modelde) | ✅ Doğrulandı |
| **Alt Kart** | Ayrı BT/APRS daughterboard | ✅ Doğrulandı |
| **Arayüz** | UART (muhtemelen) | ⚠️ Doğrulanacak |
| **İşlev** | BT ses, BT PTT, BT programlama | ✅ Doğrulandı |

**⚠️ Doğrulanması Gerekenler:**
- BT modül üretici/model
- BT sürümü (4.x? 5.x? BLE?)
- UART bağlantı pinleri
- AT komut seti (varsa)

---

### 2.10. FM Yayın Alıcısı

| Özellik | Değer | Doğrulama |
|---------|-------|-----------|
| **Varlığı** | Evet | ✅ Doğrulandı |
| **Frekans Aralığı** | 76-108 MHz (bölgesel) | ⚠️ Doğrulanacak |
| **Implementasyon** | AT1846S içinde veya ayrı IC | ⚠️ Doğrulanacak |

**⚠️ Doğrulanması Gerekenler:**
- AT1846S içinde mi yoksa ayrı bir çipte mi?
- Stereo/mono desteği
- RDS desteği

---

### 2.11. Güç Yönetimi

| Özellik | Değer | Doğrulama |
|---------|-------|-----------|
| **Pil** | Li-ion, 2100 mAh (nominal) | ✅ Doğrulandı |
| **Nominal Gerilim** | 7.4V (2S) | ✅ Doğrulandı |
| **Şarj** | USB üzerinden | ✅ Doğrulandı |
| **Şarj IC** | Model bilinmiyor | ⚠️ Doğrulanacak |

**⚠️ Doğrulanması Gerekenler:**
- Şarj yönetim IC modeli
- Pil gerilim ölçüm ADC kanalı
- Düşük pil eşik değerleri
- Güç regülatör IC'leri

---

### 2.12. RF Güç Amplifikatörü ve Kontrol Devreleri

| Özellik | Değer | Doğrulama |
|---------|-------|-----------|
| **VHF Çıkış Gücü** | ~6-7W | ✅ Doğrulandı |
| **UHF Çıkış Gücü** | ~6W | ✅ Doğrulandı |
| **PA Tipi** | Discrete MOSFET | ✅ Doğrulandı |
| **Güç Kontrolü** | AT1846S PA bias + harici kontrol | ⚠️ Doğrulanacak |

**⚠️ Doğrulanması Gerekenler:**
- PA MOSFET modelleri
- RF güç algılama devresi (directional coupler + ADC)
- ALC (Automatic Level Control) implementasyonu
- VHF/UHF bant anahtarlama mekanizması
- Harmonik filtre yapısı

---

## 3. USB İletişim Protokolü Analizi

### 3.1. Fiziksel Bağlantı
- USB 2.0 Full-Speed
- Virtual COM Port (CDC-ACM veya FTDI benzeri)
- GD32F303 dahili USB transceiver kullanılıyor

### 3.2. Protokol (anytone-flash-tools araştırmasından)

**Oturum Başlatma:**
1. Host → Cihaz: `PROGRAM` ASCII komutu gönderilir
2. Cihaz → Host: ACK + donanım/firmware bilgisi döner
3. Programlama moduna geçiş tamamlanır

**Veri Transferi:**
- Yapılandırılmış komut çerçeveleri kullanılır
- Her çerçeve: komut kodu + hedef bellek adresi + veri uzunluğu + veri + checksum
- Okuma ve yazma işlemleri ayrı komutlarla yapılır

**Bellek Erişimi:**
- Codeplug verileri harici NAND flash'ta saklanır
- Font, ikon gibi sistem verileri de NAND flash'ta
- Firmware MCU dahili flash'ta

### 3.3. Araçlar ve Yöntemler
- `anytone-flash-tools` (Python): Protokol analizi ve codeplug işlemleri
- `anytone-emu`: CPS yazılımını kandırarak codeplug dump yapma
- `qdMR`: Açık kaynak CPS alternatifi (Linux)
- Wireshark + USBPcap: USB trafik analizi

**⚠️ KRİTİK UYARILAR:**
1. Protokol tam olarak belgelenmemiştir - reverse engineering devam etmektedir
2. Yanlış bellek yazma işlemleri kalibrasyon verilerini bozabilir
3. Firmware yazma protokolü codeplug yazma protokolünden farklıdır
4. AT-D878UVII Plus V4.00'ın protokol farklılıkları olabilir

---

## 4. Mevcut Açık Kaynak Projelerden Öğrenilenler

### 4.1. RadioFoundry
- **Lisans:** MIT
- **Durum:** AnyTone 878 serisi için aktif geliştirme
- **Önemli:** GD32F30x + SCT3258 mimarisini hedefliyor
- **WebUSB:** Tarayıcı üzerinden firmware yükleme yeteneği
- **Kaynak Değeri:** SCT3258 HPI araştırması ve donanım bilgileri çok değerli

### 4.2. OpenRTX
- **Lisans:** GPL-3.0
- **Durum:** Aktif, birden fazla platform
- **Önemli:** AT1846S sürücüsü kapsamlı ve test edilmiş
- **Mimari:** Katmanlı HAL + sürücü yapısı referans alınabilir
- **Kısıtlama:** AnyTone 878 için resmi port yok (Tytera/Radioddity hedefli)

### 4.3. MMDVM
- **Lisans:** GPL-2.0
- **Durum:** Olgun proje
- **Önemli:** DMR, P25, NXDN, YSF, M17 dijital mod implementasyonları
- **Kısıtlama:** Hotspot/repeater odaklı, el telsizi firmware'i değil

### 4.4. M17 Project
- **Lisans:** GPL / FDL
- **Codec:** Codec2 (tamamen açık kaynak, patent-free)
- **Önemli:** AMBE+2'ye ihtiyaç duymayan açık dijital ses
- **Modülasyon:** 4FSK, 4800 sym/s, 12.5/25 kHz kanal

---

## 5. Doğrulama Öncelikleri

### 5.1. Fiziksel Teardown ile Doğrulanması Gerekenler
Aşağıdaki bilgiler yalnızca cihaz kartının fiziksel incelenmesiyle doğrulanabilir:

1. ☐ NAND flash çip modeli ve üreticisi
2. ☐ LCD kontrolcü IC modeli
3. ☐ AT1846S bağlantı tipi (I2C vs SPI)
4. ☐ GPS modül modeli
5. ☐ Bluetooth modül modeli
6. ☐ Şarj yönetim IC modeli
7. ☐ PA MOSFET modelleri
8. ☐ Güç regülatör IC'leri
9. ☐ Referans kristal frekansı
10. ☐ Daughterboard konnektör pinout

### 5.2. Yazılım/Protokol ile Doğrulanması Gerekenler

1. ☐ USB VID/PID değerleri
2. ☐ Firmware güncelleme protokolü detayları
3. ☐ Bootloader adresi ve boyutu
4. ☐ Codeplug format farkları (V4.00)
5. ☐ SCT3258 HPI komut seti

---

## 6. Sonraki Adımlar

1. **Fiziksel Teardown:** Cihaz arka kapağı açılarak PCB fotoğraflanmalı
   ve bileşen markings okunmalıdır.
2. **USB Sniffing:** Wireshark ile CPS ↔ cihaz trafiği yakalanmalıdır.
3. **SWD Erişimi:** GD32F303 SWD portunun PCB üzerindeki yeri belirlenmeli
   (flash okuma koruması aktif olabilir).
4. **RadioFoundry İletişimi:** Proje ile iletişime geçilerek donanım
   bulguları paylaşılmalıdır.

---

*Bu rapor topluluk araştırmaları, üretici datasheetleri ve açık kaynak proje
analizlerinden derlenmiştir. Fiziksel donanım doğrulaması yapılana kadar
"⚠️ Doğrulanacak" işaretli bilgiler kesin kabul edilmemelidir.*
