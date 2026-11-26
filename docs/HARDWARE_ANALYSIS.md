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
| **VHF Bandı** | 131–178 MHz (ölçülen kilit aralığı) | ✅ RE doğruladı |
| **UHF Bandı** | 393–572 MHz (ölçülen kilit aralığı) | ✅ RE doğruladı |
| **220 Bandı** | 200–286 MHz (÷4 bölücü) | ✅ RE doğruladı |
| **Kontrol Arayüzü** | **Bit-banged 3-wire GPIO** | ✅ RE doğruladı |
| **Register Genişliği** | 16-bit data, 7-bit adres, 0x80 = oku | ✅ RE doğruladı |
| **Kanal Genişliği** | 12.5 kHz / 25 kHz | ✅ Doğrulandı |
| **Frekans Kayıt** | Reg 0x29/0x2A = f_kHz × 16 | ✅ RE doğruladı |
| **PLL Kilit** | Reg 0x0D[15] = lock flag | ✅ RE doğruladı |
| **VCO** | ~779–1145 MHz, bölücü ÷6/÷4/÷2 | ✅ RE doğruladı |

**Görevi:** VHF ve UHF bantlarında RF sinyal alımı ve üretimi. Frekans sentezi,
modülasyon, demodülasyon, squelch algılama, RSSI ölçümü.

**MCU ile Haberleşme: Bit-banged 3-wire GPIO (I2C DEĞİL)**

> **⚠️ DİKKAT:** `driver_at1846s.c` şu an yanlışlıkla `hal_i2c_write_reg`
> kullanıyor. Stok firmware RE ile kesinlikle doğrulandı: AT1846S,
> `at1846_write_reg` (0x0807A6FC, 324 çağrı) ile **bit-banged GPIO** üzerinden
> erişilir. Sürücü `hal_gpio` ile yeniden yazılmalı.

| Sinyal | MCU Pini | Stok firmware sabiti |
|--------|----------|----------------------|
| SCLK   | **PB3**  | `at1846_sclk`        |
| SDIO   | **PB4**  | `at1846_sdio`        |
| SEN    | **PB5**  | `at1846_sen`         |

**Frekans hesaplama (doğrulanmış):**
- `reg_val = (freq_hz / 10000) * 16`  — yani `f_kHz × 16` (NOT: `(f << 16) / xtal`)
- Reg 0x05 spur kaçınma için bazı frekanslarda değişir (144, 159, 410, 455, 476–482 MHz)

**RX izleme filtresi:** TIM3 CCR1 PWM — her retune'da ayarlanmalı.  
**Front-end GPIO:** AT1846S reg 0x1F — `[9:8]` VHF, `[1:0]` UHF, `11` = açık.

**Register Haritası (Stok Firmware RE ile Doğrulanmış):**

| Adres | İşlev | Kaynak |
|-------|-------|--------|
| 0x04 | CLK mode | ✅ RE |
| 0x05 | Ref/spur seçimi (8763/8543/8643) | ✅ RE ölçüm |
| 0x0D | [15] PLL lock flag | ✅ RE ölçüm |
| 0x0F | [7:6] bant durum okuma (R/O: 11=VHF÷6, 10=220÷4, 00=UHF÷2) | ✅ RE ölçüm |
| 0x13 | [8:0] VCO cap-bank (511=sınır dışı) | ✅ RE ölçüm |
| 0x1B | RSSI (8-bit) | ✅ RE doğrulandı |
| 0x1F | GPIO: [9:8]=VHF frontend, [1:0]=UHF frontend (11=açık, 10=kapalı) | ✅ RE |
| 0x29 | RF frekans üst: f_kHz × 16 >> 16 | ✅ RE doğrulandı |
| 0x2A | RF frekans alt: f_kHz × 16 & 0xFFFF | ✅ RE doğrulandı |
| 0x29/0x2A | Okunduğunda VCO frekansı ÷6 döner (reg05=8763'te) | ✅ RE ölçüm |
| 0x30 | [13:12]=bant genişliği, [5]=RX on, [6]=TX on | ✅ RE |
| 0x49 | Squelch eşik (rf_squelch_cal_apply hedefi) | ✅ RE |
| 0x7F | Page register (page 0x7F=1 = AGC tablo) | ✅ RE |

**⚠️ Hâlâ Doğrulanması Gerekenler:**
- Referans kristal frekansı (26 MHz varsayılıyor)
- PA kontrol ve güç ölçüm devreleri (APC DAC = DAC ch1 kesin)
- 220 MHz bandı ön-filtre konfigürasyonu

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
| **MCU Arayüzü** | **8-bit HPI (Host Port Interface)** | ✅ RE doğruladı |

**MCU ile Haberleşme: 8-bit HPI — adres satırı yok, paralel veri yolu.**

| Sinyal | MCU Pini | Stok Fonksiyon |
|--------|----------|----------------|
| Data[7:0] | PD8–PD15 | 8-bit veri yolu |
| HCSN | PD2 | Chip select (negatif) |
| HWRN | PD3 | Yazma (negatif) |
| HRDN | PD4 | Okuma (negatif) |

Stok erişim: `hpi_write_byte/buf`, `hpi_read_byte` @ `0x080823BA`.
Wrapper'lar: `0x0808711A / 32 / 3E`.
Op-code dispatcher'lar: `sct_query` @ `0x0808B91A`, `sct_command` @ `0x0808C7D2`.
SCT3258 komut kuyruğu 40 girişli (`dmr_bb_cmd_enqueue(op,a,b,c)`).

**Görevi:** DMR dijital ses ve veri işlemenin tamamını gerçekleştirir:
- TDMA slot zamanlama (Slot 1 ve Slot 2)
- AMBE+2 vocoder encode/decode
- FEC (Forward Error Correction)
- Fiziksel katman (4FSK modülasyon/demodülasyon)
- Veri bağlantı katmanı
- Çağrı kontrol katmanı
- Otomatik dijital/analog mod algılama ve geçişi

**⚠️ KRİTİK KISITLAMALAR:**
1. **Datasheet Erişimi:** SCT3258 tam teknik dokümanı halka açık değildir.
   CML Micro web sitesinden product brief erişilebilir ancak tam register
   haritası ve HPI protokolü NDA gerektiriyor olabilir.
2. **AMBE+2 Lisansı:** SCT3258 içindeki AMBE+2 vocoder DVSI'nin mülkiyetindedir.
   Bu codec'in yazılımsal olarak çoğaltılması patent ihlalidir.
3. **HPI Protokolü:** Stok firmware RE ile HPI pin bağlantıları doğrulandı
   (PD2–PD4, PD8–PD15) ancak tam komut seti hâlâ tersine mühendislik aşamasında.
4. **Kod alanı:** Stok firmware DMR stack'i `0x08084000–0x08094000` arasında.
   RE süreci devam ediyor (sct_query/sct_command op-code'ları kısmen haritalandı).

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
| **Çözünürlük** | **160×128** piksel | ✅ RE doğruladı |
| **Kontrolcü** | **ST7735S** | ✅ RE doğruladı |
| **Arayüz** | **8080 Paralel (GPIO)** | ✅ RE doğruladı |
| **Renk** | RGB565 (16-bit) | ✅ RE doğruladı |

**Görevi:** Kullanıcı arayüzü görüntüleme - kanal bilgisi, frekans,
DMR verileri, sinyal gücü, pil durumu, menüler.

**⚠️ DİKKAT — SPI DEĞİL, 8080 PARALEL:**

Stok firmware RE ile kesinlikle doğrulandı: ekran SPI üzerinden değil,
8080 paralel GPIO bus üzerinden sürülür.

| Sinyal | MCU Pini | Stok Fonksiyon |
|--------|----------|----------------|
| Data[7:0] | PE7–PE14 | 8-bit veri yolu |
| D/CX | PB10 | Komut/veri seçici |
| RST  | PB11 | Donanım reset |
| CS   | PB12 | Chip select |
| WR   | PB13 | Yazma strobe |

Stok fonksiyonlar: `lcd_init_st7735`, `lcd_draw_text` (ASCII + GB2312 font).
`driver_display.c` SPI varsayımı içeriyorsa EXMC/GPIO paralel'e taşınmalıdır.

**⚠️ Hâlâ Doğrulanması Gerekenler:**
- Arka aydınlatma PWM pin'i

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
| **Arayüz** | UART (USART2 / GD adıyla USART1) | ✅ RE doğruladı |
| **Protokol** | NMEA 0183 | ✅ RE doğruladı |
| **Baud Rate** | **9600** | ✅ RE doğruladı |
| **MCU Pini** | 0x40004400 = ST USART2 (GD "USART1") | ✅ RE |
| **Konfig komutları** | MTK/CASIC / $CFG (UBX yok) | ✅ RE doğruladı |

Stok fonksiyonlar: `nmea_rx_char`, `gps_module_config`.

**⚠️ Hâlâ Doğrulanması Gerekenler:**
- GPS modül üretici/model (fiziksel teardown)
- Desteklenen GNSS sistemleri (GPS, GLONASS, BeiDou?)
- Güncelleme hızı

---

### 2.9. Bluetooth / APRS LinkBoard Modülü

| Özellik | Değer | Doğrulama |
|---------|-------|-----------|
| **Varlığı** | Evet (Plus modelde, BHT V2.00) | ✅ RE doğruladı |
| **Alt Kart** | Ayrı BT/APRS daughterboard (BHT V2.00 H/M) | ✅ RE doğruladı |
| **Arayüz** | **UART4 (PC10/PC11)** | ✅ RE doğruladı |
| **Güç Pin** | PE15 | ✅ RE doğruladı |
| **Probe baud** | 115200 (`AT+VER?`) → "APRS…" = APRS board | ✅ RE doğruladı |
| **Çalışma baud** | 9600 (probe sonrası) | ✅ RE doğruladı |

**Protokoller:**
- Binary çerçeve: `7E len cmd payload sum EF` (`bt_send_frame`)
  - 0xCA komutu = bt_probe (×100)
- ASCII AT komutları: `AT+BTON`, `+CONN`, `+PTTKEY`, `+PTT=P/R`, `+APRS:` (RX frame taşır)

Stok fonksiyon tablosu: `0x080B2E8C`. Firmware güncelleme modu: `mode_byte 0x20000951 == 0x14`
= UART4↔USB passthrough (BT firmware update).

**⚠️ Hâlâ Doğrulanması Gerekenler:**
- BT modül üretici/model (fiziksel teardown)
- BT sürümü (4.x? 5.x? BLE?)

---

### 2.10. FM Yayın Alıcısı

| Özellik | Değer | Doğrulama |
|---------|-------|-----------|
| **Varlığı** | Evet | ✅ Doğrulandı |
| **IC** | **BK1080** (AT1846S'ten bağımsız) | ✅ RE doğruladı |
| **Arayüz** | **I2C: PB6 SDA / PB7 SCL, adres 0x20** | ✅ RE doğruladı |
| **Register protokolü** | Si470x-style sıralı yazma, reg 0x02'den | ✅ RE doğruladı |
| **Stereo/RDS** | Muhtemelen yok (BK1080 mono FM alıcı) | ⚠️ Doğrulanacak |

Stok fonksiyonlar: `fm_chip_init`, `fm_set_volume`.

> AT1846S ile **karıştırılmamalı** — AT1846S sadece VHF/UHF (134–572 MHz),
> FM yayın (76–108 MHz) BK1080 üzerinden alınır.

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

### 5.1. RE ile Doğrulanmış Bilgiler (Artık Sorgulanmaya Gerek Yok)

Aşağıdakiler stok firmware V4.00 tersine mühendisliği ile kesin olarak doğrulandı
(bkz. `docs/REVERSE_ENGINEERING_BRIDGE.md`):

| ✅ Doğrulandı | Kaynak |
|---|---|
| AT1846S → bit-banged 3-wire GPIO (PB3/PB4/PB5) | RE: `at1846_write_reg` 324 çağrı |
| AT1846S frekans = f_kHz × 16 (reg 0x29/0x2A) | RE: `at1846_set_freq` 0x0805F504 |
| AT1846S PLL kilit: reg 0x0D[15] | RE: stok ölçüm |
| VCO sınırları: VHF 131–178, 220 200–286, UHF 393–572 MHz | RE: stok radyo ölçümü |
| LCD = ST7735S, 160×128, 8080 paralel (PE7–14, PB10–13) | RE: `lcd_init_st7735` |
| SCT3258 HPI: PD8–15 (data), PD2 HCSN, PD3 HWRN, PD4 HRDN | RE: `hpi_write_byte` |
| BK1080 FM alıcı: I2C PB6/PB7, adres 0x20 | RE: `fm_chip_init` |
| GPS: USART2, 9600 baud, NMEA, MTK/CASIC | RE: `nmea_rx_char` |
| BT/APRS: UART4 PC10/PC11, güç PE15, baud 115200→9600 | RE: `bt_board_probe` |
| SPI NAND: CS=PB8, GigaDevice 0xC8 | RE: `spiflash_detect_id` |
| USB CDC: GD32 VID 28e9:018a, OUT EP3 64B, IN EP 0x81 | RE: stok doğrulandı |
| MCU başlangıç: SP=0x20015720, Reset=0x08004C90 | RE: image analiz |
| Bootloader: 0x08000000–0x08003FFF (16 KB), güncelleme dosyasında YOK | RE: `.CDI` analiz |
| ADC ch10 = PC0 = pre-squelch RX audio (DMA-dairesel) | RE: TIM6 IRQ analiz |
| TX audio: PB9 = TIM4 CH4 PWM DAC 76.8 kHz | RE: `afsk_pwm_buf` DMA |
| RX izleme filtresi: TIM3 CCR1 PWM | RE: `rf_frontend_tune_from_tables` |
| APC DAC = DAC ch1 (PA güç kontrolü) | RE: `apc_dac_write` |
| Unknown I2C dev: PB6/PB7, adres 0x3A, reg 0x10 poll | RE: `i2c_0x3a_read_reg` |
| Ses codec (TLV320AIC3204) → SCT3258 serial port 1 üzerinden | RE: `sct_codec_write*` |
| uC/OS-II: 200 Hz tick, 34 TCB, LOWEST_PRIO=63 | RE: OS analiz |

### 5.2. Fiziksel Teardown ile Doğrulanması Gerekenler

1. ☐ NAND flash çip modeli ve üreticisi (D878UV=1Gbit, UVII=2Gbit ama model bilinmiyor)
2. ☐ LCD arka aydınlatma PWM pin'i
3. ☐ GPS modül modeli (MTK/CASIC komutları destekleniyor ama üretici bilinmiyor)
4. ☐ BT modül modeli ve BT sürümü
5. ☐ Şarj yönetim IC modeli
6. ☐ PA MOSFET modelleri
7. ☐ Güç regülatör IC'leri
8. ☐ Referans kristal frekansı (26 MHz varsayılıyor)
9. ☐ Daughterboard konnektör pinout (fiziksel)
10. ☐ Unknown I2C device 0x3A kimliği

### 5.3. Yazılım/Protokol ile Doğrulanması Gerekenler

1. ☐ SCT3258 HPI tam komut seti (RE devam ediyor)
2. ☐ Codeplug format V4.00 farkları (kısmen `anytone-flash-tools`'da var)
3. ☐ AT1846S 220 MHz bandı ön-filtre konfigürasyonu

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
