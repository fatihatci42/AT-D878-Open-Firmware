# AnyTone AT-D878UVII Plus Kesin Donanım Pin Haritası (Pinout)
## Kart Revizyonu: D878UV2A-BORD
**Türetilme Kaynağı:** Orijinal V4.00 Firmware Assembly Başlatma Rutini (`0x08020D14 - 0x08020EBC`)

---

## 1. Ekran (1.77" TFT LCD) Pinleri

| İşlev | Port | Pin | Yapılandırma | Başlangıç Durumu |
|---|---|---|---|---|
| **LCD Reset (RST)** | `GPIOC` | `Pin 3` | Output Push-Pull 50MHz | HIGH (Aktif) |
| **LCD Data / Command (A0 / DC)** | `GPIOC` | `Pin 4` | Output Push-Pull 50MHz | HIGH |
| **LCD Chip Select (CS)** | `GPIOC` | `Pin 5` | Output Push-Pull 50MHz | LOW (Seçili) |
| **LCD Arka Aydınlatma (BL LED)** | `GPIOC` | `Pin 10`| Output Push-Pull 50MHz | HIGH (Açık) |

---

## 2. RF Alıcı-Verici ve Güç Kontrol Pinleri

| İşlev | Port | Pin | Yapılandırma | Açıklama |
|---|---|---|---|---|
| **RF Transceiver Kontrol 1** | `GPIOB` | `Pin 1` | Output Push-Pull 50MHz | AT1846S Enable / CS |
| **RF Transceiver Kontrol 2** | `GPIOB` | `Pin 2` | Output Push-Pull 50MHz | AT1846S Serial Clock / Data |
| **VHF / UHF Bant Seçici (RF Switch)**| `GPIOE` | `Pin 6` | Output Push-Pull 50MHz | `1` = UHF (400-520 MHz), `0` = VHF (134-174 MHz) |
| **RF PA Bias / TX Enable** | `GPIOE` | `Pin 7` | Output Push-Pull 50MHz | `1` = TX Güç Katı Aktif, `0` = TX Kapalı |

---

## 3. Ses ve Çevre Birim Pinleri

| İşlev | Port | Pin | Yapılandırma | Açıklama |
|---|---|---|---|---|
| **Ses Hoparlör Amplifikatörü (Mute)**| `GPIOA` | `Pin 7` | Output Push-Pull 50MHz | `1` = Ses Açık, `0` = Mute |
| **Bluetooth & GPS Güç Anahtarı** | `GPIOE` | `Pin 9` | Output Push-Pull 50MHz | `1` = Modüllere Güç Ver |
| **USART0 TX (Debug Konsol)** | `GPIOA` | `Pin 8` | Alt-Func Push-Pull | Seri Port Veri Gönderim |
| **USART0 RX (Debug Konsol)** | `GPIOA` | `Pin 9` | Input Pull-Up | Seri Port Veri Alım |

---

## 4. Kullanıcı Girişleri (PTT & Tuşlar)

| İşlev | Port | Pin | Yapılandırma | Açıklama |
|---|---|---|---|---|
| **PTT Girişi** | `GPIOA` | `Pin 0` | Input Pull-Up | Basıldığında `0` (GND), Bırakıldığında `1` |
| **Tuş Takımı / Enkoder Faz** | `GPIOA` | `Pin 14`| Input Pull-Up | Döner pot ve matris tarama |

---

## 5. DMR Baseband (SCT3258) Veri Yolu

| İşlev | Port | Pin | Yapılandırma | Açıklama |
|---|---|---|---|---|
| **SCT3258 Reset / Enable** | `GPIOE` | `Pin 0` | Output Push-Pull 50MHz | `1` = Çalışma Modu, `0` = Reset |
| **SCT3258 HPI Kontrol** | `GPIOE` | `Pin 1` | Output Push-Pull 50MHz | HPI Read/Write yönlendirme |
| **SCT3258 8-Bit Paralel Veri (D0-D7)**| `GPIOE`| `Pin 0 - Pin 7` | Çok Fonksiyonlu I/O | HPI Paralel Veri Yolu |
