# AT-D878 Open Firmware - Yazılım Mimarisi

## Genel Bakış

Firmware, katmanlı bir mimari ile tasarlanmıştır. Her katman yalnızca altındaki
katmanla iletişim kurar. Donanım bağımlılıkları HAL katmanının arkasında izole edilir.

```
┌─────────────────────────────────────────────────┐
│              APPLICATION LAYER                   │
│  ┌──────────┐ ┌──────────┐ ┌─────────────────┐  │
│  │    UI    │ │  Config  │ │   CPS Protocol  │  │
│  └──────────┘ └──────────┘ └─────────────────┘  │
├─────────────────────────────────────────────────┤
│              PROTOCOL LAYER                      │
│  ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐  │
│  │ DMR  │ │ M17  │ │ APRS │ │ FM   │ │ ...  │  │
│  └──────┘ └──────┘ └──────┘ └──────┘ └──────┘  │
├─────────────────────────────────────────────────┤
│              RADIO CORE                          │
│  ┌──────────┐ ┌──────────┐ ┌─────────────────┐  │
│  │   VFO   │ │ Channel  │ │   TX/RX Engine  │  │
│  └──────────┘ └──────────┘ └─────────────────┘  │
├─────────────────────────────────────────────────┤
│              DEVICE DRIVERS                      │
│  ┌────────┐ ┌────────┐ ┌────────┐ ┌──────────┐  │
│  │AT1846S │ │SCT3258 │ │Display │ │  Audio   │  │
│  │        │ │        │ │        │ │AIC3204   │  │
│  └────────┘ └────────┘ └────────┘ └──────────┘  │
│  ┌────────┐ ┌────────┐ ┌────────┐ ┌──────────┐  │
│  │  GPS   │ │Keypad  │ │  BT    │ │   NAND   │  │
│  └────────┘ └────────┘ └────────┘ └──────────┘  │
├─────────────────────────────────────────────────┤
│        HARDWARE ABSTRACTION LAYER (HAL)          │
│  ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐      │
│  │GPIO │ │UART │ │SPI  │ │I2C  │ │USB  │      │
│  └─────┘ └─────┘ └─────┘ └─────┘ └─────┘      │
│  ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐      │
│  │DMA  │ │Timer│ │ADC  │ │RTC  │ │EXMC │      │
│  └─────┘ └─────┘ └─────┘ └─────┘ └─────┘      │
├─────────────────────────────────────────────────┤
│              RTOS / KERNEL                       │
│         (FreeRTOS veya Zephyr RTOS)              │
├─────────────────────────────────────────────────┤
│              BOOTLOADER                          │
│    (Mevcut üretici bootloader korunur)            │
├─────────────────────────────────────────────────┤
│           GD32F303VGT6 HARDWARE                  │
│    ARM Cortex-M4 @ 120MHz | 1MB Flash | 96KB RAM │
└─────────────────────────────────────────────────┘
```

## Katman Tanımları

### 1. Bootloader Katmanı
- Mevcut üretici bootloader'ı **ilk aşamada korunur**
- Firmware bütünlük kontrolü (CRC32/SHA256)
- Güvenli güncelleme mekanizması
- Kurtarma modu (factory reset)
- Orijinal firmware'e geri dönüş yeteneği

### 2. HAL Katmanı
GD32F303 Standard Peripheral Library üzerine inşa edilir.

| Modül | Dosya | Açıklama |
|-------|-------|----------|
| GPIO | `hal_gpio.c/h` | Pin konfigürasyonu ve kontrolü |
| UART | `hal_uart.c/h` | Seri haberleşme (GPS, BT, Debug) |
| SPI | `hal_spi.c/h` | AT1846S, NAND, Display |
| I2C | `hal_i2c.c/h` | Audio codec, EEPROM |
| USB | `hal_usb.c/h` | CPS haberleşme, DFU |
| DMA | `hal_dma.c/h` | Ses ve veri transferleri |
| Timer | `hal_timer.c/h` | PWM, zamanlayıcılar |
| ADC | `hal_adc.c/h` | Pil, RSSI, sıcaklık |
| RTC | `hal_rtc.c/h` | Gerçek zamanlı saat |
| EXMC | `hal_exmc.c/h` | Harici bellek kontrolcüsü |
| WDT | `hal_watchdog.c/h` | Watchdog timer |
| NVIC | `hal_interrupt.c/h` | Kesme yönetimi |

### 3. Device Drivers Katmanı

| Sürücü | Bileşen | Arayüz | Durum |
|--------|---------|--------|-------|
| `driver_at1846s` | RF Transceiver | I2C/SPI | Araştırılıyor |
| `driver_sct3258` | DMR Baseband | HPI (Paralel) | Araştırılıyor |
| `driver_display` | TFT LCD 1.77" | SPI | Araştırılıyor |
| `driver_keyboard` | Tuş matrisi + Encoder | GPIO + ADC | Araştırılıyor |
| `driver_audio` | TLV320AIC3204 | I2C + I2S | Araştırılıyor |
| `driver_gps` | GPS modülü | UART | Araştırılıyor |
| `driver_bluetooth` | BT modülü | UART | Araştırılıyor |
| `driver_nand` | Harici Flash | SPI/EXMC | Araştırılıyor |
| `driver_fm_receiver` | FM Broadcast RX | - | Araştırılıyor |

### 4. Radio Core Katmanı
Frekans yönetimi, TX/RX durumu, squelch, CTCSS/DCS, güç kontrolü,
kanal/VFO yönetimi gibi temel telsiz işlevlerini barındırır.

### 5. Protocol Layer
Her dijital protokol bağımsız modül olarak tasarlanır:
- `digital_dmr.c/h` - DMR Tier II
- `digital_m17.c/h` - M17 Protocol
- `digital_aprs.c/h` - APRS
- Diğerleri fizibiliteye göre eklenecek

### 6. Application Layer
UI rendering, menü sistemi, yapılandırma yönetimi ve CPS haberleşme
protokolünü içerir.

## Bellek Haritası (Planlanan)

```
GD32F303VGT6 Flash (1 MB = 0x00000000 - 0x000FFFFF)
┌──────────────────────────────────────┐ 0x08000000
│  Bootloader (mevcut/korunacak)       │ ~16 KB
├──────────────────────────────────────┤ 0x08004000
│  Firmware Application                │ ~896 KB
│  ├── .text (kod)                     │
│  ├── .rodata (sabitler, font, dil)   │
│  └── .data (başlangıç değerleri)     │
├──────────────────────────────────────┤ 0x080E0000
│  Configuration Storage               │ ~64 KB
│  ├── Kullanıcı ayarları              │
│  ├── Kalibrasyon verisi              │
│  └── Yedek yapılandırma              │
├──────────────────────────────────────┤ 0x080F0000
│  Recovery / Flag Area                │ ~64 KB
└──────────────────────────────────────┘ 0x08100000

GD32F303VGT6 SRAM (96 KB = 0x20000000 - 0x20017FFF)
┌──────────────────────────────────────┐ 0x20000000
│  .bss + .data                        │
├──────────────────────────────────────┤
│  Heap (dinamik bellek)               │
├──────────────────────────────────────┤
│  RTOS Task Stacks                    │
├──────────────────────────────────────┤
│  Audio Buffers (DMA)                 │
├──────────────────────────────────────┤ 0x20017FFF
│  Stack (MSP)                         │
└──────────────────────────────────────┘

Harici NAND Flash (256 MB - AT-D878UVII Plus)
┌──────────────────────────────────────┐ 0x00000000
│  Fontlar (Çince, Kiril vb.)         │ ~1.3 MB
├──────────────────────────────────────┤ 0x00150000
│  Boot Splash Bitmap                  │ ~40 KB
├──────────────────────────────────────┤ 0x0015A000
│  UI İkonları ve Grafikler            │ ~680 KB
├──────────────────────────────────────┤ 0x00200000
│  Codeplug / Kullanıcı Verisi         │
│  ├── Kanallar (4000)                 │
│  ├── Bölgeler                        │
│  ├── Scan listeleri                  │
│  ├── DMR ID DB (500K)               │
│  ├── Talkgroup / Contact             │
│  └── GPS / APRS ayarları            │
├──────────────────────────────────────┤
│  Ses kayıtları                       │
├──────────────────────────────────────┤
│  Boş alan                            │
└──────────────────────────────────────┘
```

## RTOS Seçimi

İki aday değerlendiriliyor:

| Özellik | FreeRTOS | Zephyr RTOS |
|---------|----------|-------------|
| GD32F303 Desteği | Dolaylı (STM32 uyumu) | Resmi BSP mevcut |
| Bellek Kullanımı | Düşük (~6-10 KB) | Orta (~20-40 KB) |
| Lisans | MIT | Apache 2.0 |
| Sürücü Ekosistemi | Minimal | Kapsamlı |
| Öğrenme Eğrisi | Düşük | Orta-Yüksek |
| USB Stack | Harici | Dahili |
| BLE Stack | Yok | Dahili |

**Öneri:** İlk prototip için FreeRTOS (düşük bellek kullanımı, hızlı geliştirme).
Uzun vadede Zephyr'e geçiş değerlendirilebilir.

## Derleme Sistemi

- **Toolchain:** ARM GCC (arm-none-eabi-gcc)
- **Build System:** CMake + Ninja
- **Platform:** Windows (öncelikli), Linux, macOS
- **Debug:** OpenOCD + SWD (donanım doğrulandıktan sonra)
