# AnyTone AT-D878UVII Plus Donanım ve Kart Revizyon Belgesi
## Kart Kodu: D878UV2A-BORD

**Analiz Tarihi:** 2026-11-22  
**Kaynak:** Resmi Firmware V4.00 (2025/07/25) Binary İncelemesi  
**Yöntem:** Tersine Mühendislik & Binary Ayrıştırma (Non-destructive)

---

## 1. Kesinleşen Donanım Mimarisi

| Bileşen | Orijinal Firmware'de Doğrulanan Değer | Açıklama |
|---|---|---|
| **Ana Kart Revizyonu** | **`D878UV2A-BORD`** | Firmware binary içinde `0x080DDB96` adresinde tespit edildi. |
| **Model Tanımlayıcısı** | **`D878UVII` / `D878UV2`** | `0x080A138A` ve `0x080AAE58` adreslerinde doğrulandı. |
| **MCU Ailesi** | **GigaDevice GD32F303** | `0x08081B8B`: *"GD32 Virtual ComPort in FS Mode"*, *"GD32Microelectronics"* dizgileri doğrudan USB descriptor tablosunda bulundu. |
| **DMR Baseband İşlemcisi** | **Sicomm/CML SCT3258** | `0x080E0934`: *"Sct 3258 Ver:"*, `0x080426C8`: *"for Sct!!!"* strings bulundu. |
| **RF Transceiver** | **AT1846N / AT1846S** | `0x080A139A`: *"878# 1846N"* referansı ile kesinleşti. |
| **GPS & Konum** | **NMEA 0183 / MIC-E** | GPS RSSI, NMEA Report ve MIC-E APRS alt sistemleri doğrulandı. |
| **Bluetooth** | **Dahili BT Modülü** | 140'tan fazla BT kontrol ve AT komut eşleşmesi bulundu. |

---

## 2. Bellek ve Bootloader Düzeni

```
GD32F303VGT6 Flash Bellek (1024 KB):
┌──────────────────────────────┐ 0x08000000
│  Üretici Bootloader'ı        │ (İlk 16 KB: 0x08000000 - 0x08003FFF)
├──────────────────────────────┤ 0x08004000
│  Uygulama Vektör Tablosu     │ (MSP: 0x20015858, Reset: 0x08004C91)
│                              │
│  V4.00 Firmware Görüntüsü    │ Toplam Boyut: 931.392 Byte (~909.5 KB)
│  (Kod, Sabitler, Sürücüler)  │ 
│                              │
├──────────────────────────────┤ 0x080E7400
│  Kalan Boş / Konfigürasyon   │ (~92 KB Boş alan)
└──────────────────────────────┘ 0x08100000 (1 MB Sınırı)
```

---

## 3. GD32F303 Donanım Çevre Birimi Kullanım İstatistikleri

Orijinal firmware kodunun doğrudan bellek adresleriyle eriştiği çevre birimleri:

* **Port A (GPIOA - 0x40010800):** 45 doğrudan referans (LED'ler, Encoder, Temel I/O)
* **Port B (GPIOB - 0x40010C00):** 30 doğrudan referans (I2C / SPI bus hatları)
* **Port C (GPIOC - 0x40011000):** 21 doğrudan referans (Tuş takımı satır/sütun)
* **Port D (GPIOD - 0x40011400):** 7 doğrudan referans
* **Port E (GPIOE - 0x40011800):** 37 doğrudan referans (SCT3258 HPI paralel veri/kontrol hattı)
* **USART0 (0x40013800):** Debug / Test portu
* **USART1 (0x40004400):** GPS modülü seri portu
* **UART3 / UART4:** Bluetooth ve yardımcı veri hatları
* **EXMC (0xA0000000):** Harici bellek kontrolcüsü (256 MB SPI/NAND Flash)
