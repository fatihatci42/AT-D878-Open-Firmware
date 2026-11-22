# AnyTone AT-D878UVII Plus Donanımsal Önyükleme (Boot) ve Güncelleme Modları
**Kaynak:** Resmi V4.00 Dağıtım Belgeleri (`D878UVII PLUS V4.00 official release 250815`)

---

AnyTone AT-D878UVII Plus içerisinde 4 farklı bağımsız işlemci/kart bulunmaktadır.
Her birinin ayrı açılış kombinasyonu ve USB güncelleme protokolü vardır:

```
                                  ┌───────────────────────────┐
                                  │   AnyTone AT-D878UVII     │
                                  └─────────────┬─────────────┘
                                                │
         ┌───────────────────┬──────────────────┴─────────────────┬───────────────────┐
         │                   │                                    │                   │
         ▼                   ▼                                    ▼                   ▼
  1. ANA MCU (GD32)   2. HARİCİ FLASH İKONLAR             3. DMR BASEBAND (SCT3258) 4. BT & APRS KART (LinkBoard)
┌──────────────────┐ ┌────────────────────────┐          ┌───────────────────────┐ ┌────────────────────────────┐
│ PTT + PF3 (Üst)  │ │ PTT + PF2 (Alt Yan)    │          │ # Tuşu + PF3 (Üst)    │ │ PF3 (Üst) + PF2 (Alt Yan)  │
│ Kırmızı LED Blink│ │ Ekranda: "UPDATE MODE" │          │ "This is Boot Mode    │ │ "UPDATE MODE FOR           │
│                  │ │                        │          │  for Sct!!!"          │ │  LinkBoard"                │
└──────────────────┘ └────────────────────────┘          └───────────────────────┘ └────────────────────────────┘
```

---

## 1. Ana MCU Firmware Güncelleme Modu (GD32F303)
* **Tuş Kombinasyonu:** Telsiz kapalıyken **`PTT + PF3 (Üst Turuncu/Yeşil Buton)`** basılı tutularak telsiz açılır.
* **Cihaz Durumu:** Ekran kararır, ses potunun yanındaki **Kırmızı LED yanıp söner**.
* **Yüklenen Dosyalar:** `.CDD` (Ham binary), `.CDI` (Manifest), `.spi` (Model Başlığı).
* **Varsayılan Baud Hızı:** 921600 baud.
* **Hedef Adres:** `0x08004000` (MCU Dahili Flash).

---

## 2. İkon ve Başlangıç Ekranı (NAND Flash) Güncelleme Modu
* **Tuş Kombinasyonu:** Telsiz kapalıyken **`PTT + PF2 (En alttaki yan buton)`** basılı tutularak telsiz açılır.
* **Cihaz Durumu:** LCD ekranda **`UPDATE MODE`** yazar.
* **Yüklenen Dosyalar:** `D878_ICON_V1.23.CDD` (3.7 MB ikon ve font kütüphanesi) veya özel başlangıç ekranı BMP dosyaları (160x128 16-bit BMP, 21.558 byte).
* **Hedef:** Harici 256 MB SPI Flash.

---

## 3. DMR Baseband İşlemcisi (SCT3258) Güncelleme Modu
* **Tuş Kombinasyonu:** Telsiz kapalıyken **`# tuşu + PF3 (Üst buton)`** basılı tutularak telsiz açılır.
* **Cihaz Durumu:** Ekranda **`WARNING This is Boot Mode for Sct!!!`** yazar.
* **Açıklama:** Bu modda ana MCU, USB seri portunu doğrudan SCT3258 baseband işlemcisinin HPI/seri portuna köprüler (Bypass Bridge).
* **Yüklenen Dosyalar:** SCT3258 `.hex` firmware dosyaları.

---

## 4. Bluetooth & Analog APRS Alt Kartı (LinkBoard / BHT) Güncelleme Modu
* **Tuş Kombinasyonu:** Telsiz kapalıyken **`PF3 (Üst buton) + PF2 (Alt yan buton)`** basılı tutularak telsiz açılır.
* **Cihaz Durumu:** Ekranda **`UPDATE MODE FOR LinkBoard`** yazar.
* **Kullanılan Kart Sürümleri:** 
  * `V2.00(H)`: Yeni BHT APRS alt kartı (`BHT_BOARD_H_V2.00_20230417.CDD`).
  * `V2.00(M)`: Eski BHT APRS alt kartı (`BHT_BOARD_V2.00_M_20230923.CDD`).
* **İşlev:** Analog 1200 baud APRS RX demodülatör ve Bluetooth ses/PTT firmware'i.

---

## 5. Fabrika Sıfırlama (MCU Reset)
* **Tuş Kombinasyonu:** Telsiz kapalıyken **`PTT + PF1 (Üst yan buton)`** basılı tutularak telsiz açılır.
* **Cihaz Durumu:** Ekranda *"Booting... Please Wait..."* ardından **`Are you sure you want to Initialize Radio?`** sorusu çıkar.
* **İşlev:** Tüm ayarları fabrika değerlerine döndürür, kalibrasyon ve codeplug tablolarını sıfırlar.
