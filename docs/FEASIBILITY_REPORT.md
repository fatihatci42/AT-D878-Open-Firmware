# AT-D878 Open Firmware - Teknik Fizibilite Raporu

**Cihaz:** AnyTone AT-D878UVII Plus V4.00  
**Rapor Tarihi:** 2026-11-22  
**Durum:** İlk Değerlendirme

---

## 1. Genel Fizibilite Özeti

```
┌─────────────────────────────────────────────────────────────────┐
│  ÖZELLİK                     │ FİZİBİLİTE │ RİSK SEVİYESİ    │
├─────────────────────────────────────────────────────────────────┤
│  Temel Sistem (Boot, UI)      │  ✅ YÜKSEK  │  🟢 DÜŞÜK        │
│  Analog FM TX/RX              │  ✅ YÜKSEK  │  🟡 ORTA          │
│  DMR Tier II (SCT3258 ile)    │  🟡 ORTA    │  🔴 YÜKSEK        │
│  DMR (SCT3258 bypass)         │  🔴 DÜŞÜK   │  🔴 ÇOK YÜKSEK   │
│  GPS / NMEA                   │  ✅ YÜKSEK  │  🟢 DÜŞÜK        │
│  APRS TX                      │  ✅ YÜKSEK  │  🟡 ORTA          │
│  APRS RX                      │  🟡 ORTA    │  🟡 ORTA          │
│  M17 Protocol                 │  🟡 ORTA    │  🟡 ORTA          │
│  P25 Phase 1                  │  🔴 DÜŞÜK   │  🔴 YÜKSEK        │
│  NXDN                         │  🔴 DÜŞÜK   │  🔴 YÜKSEK        │
│  YSF (System Fusion)          │  🔴 DÜŞÜK   │  🔴 YÜKSEK        │
│  D-STAR                       │  🔴 DÜŞÜK   │  🔴 YÜKSEK        │
│  Bluetooth                    │  🟡 ORTA    │  🟡 ORTA          │
│  FM Broadcast RX              │  ✅ YÜKSEK  │  🟢 DÜŞÜK        │
│  CPS Yazılımı (Studio)        │  ✅ YÜKSEK  │  🟡 ORTA          │
│  Orijinal FW'ye Geri Dönüş    │  ✅ YÜKSEK  │  🟡 ORTA          │
└─────────────────────────────────────────────────────────────────┘
```

---

## 2. Detaylı Fizibilite Analizleri

### 2.1. Temel Sistem (Boot, HAL, UI, Tuş Takımı)

**Fizibilite: ✅ YÜKSEK | Risk: 🟢 DÜŞÜK**

**Gerekçe:**
- GD32F303VGT6 iyi belgelenmiş bir MCU'dur
- Resmi GD32 peripheral library mevcut
- Zephyr RTOS resmi BSP desteği var
- OpenRTX projesi GD32 platform portları içeriyor
- 1 MB Flash + 96 KB SRAM temel sistem için yeterli

**Engeller:**
- LCD kontrolcü modeli bilinmiyor (teardown gerekli)
- Tuş matrisi pin atamaları bilinmiyor (teardown gerekli)
- Flash okuma koruması aktif olabilir (SWD erişimi kısıtlı)

**Eylem Planı:**
1. GD32F303 peripheral library ile temel HAL oluştur
2. SWD üzerinden minimal "hello world" firmware yükle
3. GPIO pinleri tarayarak LCD ve tuş bağlantılarını keşfet
4. Basit ekran sürücüsü ve menü sistemi geliştir

**Tahmini Zorluk:** ★★☆☆☆

---

### 2.2. Analog FM TX/RX

**Fizibilite: ✅ YÜKSEK | Risk: 🟡 ORTA**

**Gerekçe:**
- AT1846S tek çipli transceiver, analog FM doğal olarak destekliyor
- Register haritası büyük ölçüde belgelenmiş
- OpenRTX'te kapsamlı AT1846S sürücüsü mevcut
- Frekans ayarı, squelch, ses kontrolü doğrudan register erişimiyle yapılır

**Engeller:**
- AT1846S'in MCU'ya bağlantı tipi (I2C/SPI) doğrulanmalı
- PA kontrol devresi haritalanmalı
- CTCSS/DCS implementasyonu AT1846S + yazılım kombinasyonu gerektirir
- RF kalibrasyon parametreleri orijinal firmware'den alınmalı

**Risk Detayı:**
- Yanlış PA kontrolü RF çıkış gücünün aşılmasına veya donanım hasarına neden olabilir
- Kalibrasyon verisi kaybı durumunda telsiz uyumsuz RF çıkışı üretebilir

**Eylem Planı:**
1. AT1846S I2C/SPI bağlantısını doğrula
2. Register okuma testleri ile çip kimliğini doğrula
3. RX modunda frekans ayarı ve ses çıkışı test et
4. RSSI ve squelch kalibrasyonu yap
5. TX testlerini dummy load ile gerçekleştir
6. Güç seviyesi kalibrasyonu yap

**Tahmini Zorluk:** ★★★☆☆

---

### 2.3. DMR Tier II (SCT3258 ile)

**Fizibilite: 🟡 ORTA | Risk: 🔴 YÜKSEK**

**Gerekçe:**
- SCT3258 DMR işlemenin büyük kısmını donanımda yapıyor
- RadioFoundry bu çip üzerinde aktif çalışıyor
- AMBE+2 vocoder çip içinde gömülü (lisanslı donanım)

**Engeller:**
1. **HPI Protokolü Belgesiz:** SCT3258'in Host Port Interface'i tam olarak
   belgelenmemiş. CML Micro NDA gerektirebilir.
2. **Kayıt Erişimi:** Register haritası halka açık değil.
3. **Zamanlama Kritik:** TDMA slot zamanlaması mikrosaniye hassasiyetinde.
4. **Vocoder Lisansı:** AMBE+2 DVSI patentli. SCT3258 içindeki donanım
   implementasyonu cihazla birlikte lisanslanmış, ancak üçüncü parti
   yazılımın bu donanımı kullanma hakkı belirsiz.

**Olası Yaklaşımlar:**
- **Yaklaşım A:** RadioFoundry'nin SCT3258 araştırmalarını takip et ve
  işbirliği yap. HPI protokolü çözüldüğünde DMR desteği ekle.
- **Yaklaşım B:** CML Micro ile iletişime geçerek geliştirici erişimi iste.
- **Yaklaşım C:** SCT3258'i bypass et (aşağıdaki bölüme bakın).

**Tahmini Zorluk:** ★★★★★

---

### 2.4. DMR (SCT3258 Bypass - Yazılımsal)

**Fizibilite: 🔴 DÜŞÜK | Risk: 🔴 ÇOK YÜKSEK**

**Gerekçe:**
SCT3258'i tamamen bypass ederek MCU üzerinden DMR baseband işleme yapmak
teorik olarak mümkün olabilir ancak ciddi engeller mevcuttur.

**Engeller:**
1. **İşlem Gücü:** Cortex-M4 @ 120 MHz, DMR 4FSK modülasyon/demodülasyon,
   TDMA zamanlama VE vocoder işlemeyi aynı anda kaldırmayabilir.
2. **AMBE+2 Codec:** Yazılımsal AMBE+2 çözümü patent ihlalidir.
   mbelib gibi açık kaynak uygulamalar yasal gri alandadır.
3. **Ses Yolu:** AT1846S'in baseband ses çıkışının doğrudan MCU'ya
   yönlendirilebilip yönlendirilemeyeceği belirsizdir.
4. **Zamanlama:** DMR TDMA 30 ms çerçeve yapısı, mikrosaniye
   hassasiyetinde slot senkronizasyonu gerektirir.

**Sonuç:** Bu yaklaşım şu an için **önerilmemektedir**.

---

### 2.5. M17 Protocol

**Fizibilite: 🟡 ORTA | Risk: 🟡 ORTA**

**Gerekçe:**
- M17 tamamen açık kaynak ve patent-free
- Codec2 vocoder açık kaynak ve düşük kaynak tüketimli
- 4FSK modülasyon, AT1846S ile uyumlu olabilir
- OpenRTX'te M17 implementasyonu mevcut

**Engeller:**
1. **Baseband Erişimi:** M17'nin 4FSK sinyalini doğrudan AT1846S'e
   göndermek veya almak için baseband ses yoluna erişim gerekli.
   SCT3258'in bu yolu nasıl etkilediği belirsiz.
2. **Codec2 Performansı:** Cortex-M4 @ 120 MHz Codec2 için yeterli
   olmalı (Codec2 STM32F4 üzerinde test edilmiş).
3. **Ses Yönlendirme:** SCT3258'i bypass ederek AT1846S baseband'ine
   doğrudan erişim sağlanmalı.

**Olası Yaklaşım:**
1. AT1846S'in 9600 baud veri modu veya doğrudan baseband erişimini araştır
2. SCT3258'i analog mod'da bırakarak ses yolunu yönlendir
3. MCU üzerinde M17 4FSK modülasyon/demodülasyon + Codec2 çalıştır
4. DMA + timer ile hassas zamanlama sağla

**Tahmini Zorluk:** ★★★★☆

---

### 2.6. P25, NXDN, YSF, D-STAR

**Fizibilite: 🔴 DÜŞÜK | Risk: 🔴 YÜKSEK**

**Gerekçe:**
Bu protokollerin tamamı AMBE/IMBE vocoder ailesine bağımlıdır:

| Protokol | Vocoder | Lisans Durumu |
|----------|---------|---------------|
| P25 Phase 1 | IMBE | DVSI Patentli |
| P25 Phase 2 | AMBE+2 | DVSI Patentli |
| NXDN | AMBE+2 | DVSI Patentli |
| YSF | AMBE+2 (VW/DN) | DVSI Patentli |
| D-STAR | AMBE | DVSI Patentli |

**Engeller:**
1. Tüm vocoder'lar DVSI patent koruması altında
2. SCT3258 yalnızca DMR/dPMR AMBE+2 formatını destekliyor
3. P25 C4FM, NXDN FSK, YSF C4FM, D-STAR GMSK modülasyonu gerektirir
4. AT1846S'in bu modülasyon tiplerini doğrudan destekleyip
   desteklemediği belirsiz

**Sonuç:** Bu protokoller **kısa-orta vadede uygulanabilir değildir**.
Yalnızca araştırma ve fizibilite düzeyinde değerlendirilmelidir.

---

### 2.7. GPS ve APRS

**Fizibilite: ✅ YÜKSEK | Risk: 🟢 DÜŞÜK (GPS) / 🟡 ORTA (APRS)**

**GPS:**
- Standart UART + NMEA protokolü
- Çok sayıda açık kaynak NMEA parser mevcut
- Donanım bağımsız, iyi tanımlanmış arayüz

**APRS TX:**
- APRS paketi 1200 baud AFSK (Bell 202) ile gönderilir
- AT1846S üzerinden FM ses olarak modüle edilebilir
- AX.25 protokol implementasyonu iyi belgelenmiş

**APRS RX:**
- Daughterboard üzerinde FSK demodülatör olabilir
- Alternatif olarak ses sinyalinden yazılımsal decode
- MCU'da yeterli işlem gücü mevcut

**Tahmini Zorluk:** ★★☆☆☆ (GPS) / ★★★☆☆ (APRS)

---

### 2.8. CPS Yazılımı (AT-D878 Studio)

**Fizibilite: ✅ YÜKSEK | Risk: 🟡 ORTA**

**Gerekçe:**
- anytone-flash-tools USB protokolünü büyük ölçüde çözmüş
- Python + PySide6 ile modern GUI oluşturulabilir
- PySerial ile seri haberleşme iyi destekleniyor
- qdMR projesi codeplug format referansı sağlıyor

**Engeller:**
1. USB protokolü %100 belgelenmiş değil
2. V4.00 firmware'e özgü farklar olabilir
3. Codeplug format sürüm farklılıkları
4. Firmware güncelleme protokolü ayrı araştırma gerektirir

**Eylem Planı:**
1. USB trafiğini Wireshark ile yakala ve analiz et
2. anytone-flash-tools ile temel okuma/yazma testleri yap
3. Codeplug format haritalama dokümanı oluştur
4. PySide6 ile GUI iskeletini geliştir
5. Önce okuma, sonra yazma işlevlerini ekle

**Tahmini Zorluk:** ★★★☆☆

---

## 3. Lisans ve Patent Analizi

### 3.1. Kullanılabilecek Açık Kaynak Lisansları

| Proje | Lisans | Kullanım Durumu |
|-------|--------|-----------------|
| RadioFoundry | MIT | ✅ Referans ve kod alıntısı uygun |
| OpenRTX | GPL-3.0 | ✅ GPL-3.0 uyumlu projede kullanılabilir |
| FreeRTOS | MIT | ✅ Herhangi bir projede kullanılabilir |
| Zephyr RTOS | Apache 2.0 | ✅ GPL-3.0 ile uyumlu |
| M17 Spec | FDL | ✅ Spesifikasyon referansı serbestçe kullanılabilir |
| Codec2 | LGPL-2.1 | ✅ Firmware'de kullanılabilir |
| MMDVM | GPL-2.0 | ⚠️ GPL-3.0 ile tek yönlü uyum (dikkatli kullanım) |
| GD32 SPL | Proprietary (Free) | ✅ Kullanımı serbest |

### 3.2. Patent Kısıtlamaları

| Teknoloji | Patent Sahibi | Durum |
|-----------|---------------|-------|
| AMBE+2 Vocoder | DVSI | 🔴 Patent koruması altında |
| IMBE Vocoder | DVSI | 🔴 Patent koruması altında |
| AMBE Vocoder | DVSI | 🔴 Patent koruması altında |
| Codec2 | - | ✅ Patent-free, açık kaynak |
| DMR Protokol | ETSI | ✅ Standart, telif ücretsiz |
| M17 Protokol | M17 Project | ✅ Açık kaynak, patent-free |
| AX.25 / APRS | - | ✅ Açık standart |

### 3.3. Dağıtım Kısıtlamaları

Açık kaynak projede **asla** dağıtılmayacaklar:
- ❌ Orijinal AnyTone firmware binary dosyaları
- ❌ AMBE+2/IMBE vocoder kaynak kodu veya binary'leri
- ❌ SCT3258 NDA altındaki teknik dokümanlar
- ❌ AnyTone CPS yazılımının kaynak kodu veya binary'leri
- ❌ Kullanıcıların kişisel kalibrasyon verileri

---

## 4. Kaynak Gereksinimleri (MCU)

### 4.1. Bellek Tahmini

| Modül | Flash (tahmini) | RAM (tahmini) |
|-------|-----------------|---------------|
| Bootloader (mevcut) | ~16 KB | - |
| RTOS Kernel | ~20 KB | ~8 KB |
| HAL + Drivers | ~60 KB | ~4 KB |
| Radio Core | ~40 KB | ~8 KB |
| UI + Fonts | ~200 KB | ~16 KB |
| DMR Engine | ~80 KB | ~12 KB |
| M17 + Codec2 | ~60 KB | ~8 KB |
| GPS + APRS | ~30 KB | ~4 KB |
| Config System | ~20 KB | ~4 KB |
| CPS Protocol | ~20 KB | ~4 KB |
| **Toplam** | **~546 KB** | **~68 KB** |
| **Mevcut** | **1024 KB** | **96 KB** |
| **Kalan** | **~478 KB** | **~28 KB** |

> ⚠️ Bu tahminler yaklaşıktır. Gerçek değerler geliştirme sırasında ölçülecektir.
> RAM kullanımı kritik olabilir. DMA bufferları ve audio işleme ek RAM gerektirir.

### 4.2. İşlem Gücü Tahmini

| Görev | CPU Yükü (tahmini) |
|-------|---------------------|
| RTOS Scheduler | %2-5 |
| UI Rendering | %5-10 |
| AT1846S Kontrolü | %1-2 |
| GPS/NMEA Parsing | %1-2 |
| Codec2 (M17) | %15-25 |
| DMR Baseband (SCT3258) | %5-10 (HPI yönetimi) |
| APRS Encode/Decode | %5-10 (aktifken) |
| **Toplam (en kötü durum)** | **~55-65%** |

> Cortex-M4 @ 120 MHz, DSP ve FPU ile birlikte temel görevler için yeterli
> görünmektedir. Ancak aynı anda birden fazla dijital mod çalıştırmak
> mümkün olmayabilir.

---

## 5. Risk Matrisi

| # | Risk | Olasılık | Etki | Hafifletme |
|---|------|----------|------|------------|
| R1 | SCT3258 HPI protokolü çözülemez | ORTA | YÜKSEK | RadioFoundry işbirliği, CML iletişimi |
| R2 | Flash okuma koruması aktif | YÜKSEK | YÜKSEK | SWD debug, JTAG, bootloader analizi |
| R3 | Kalibrasyon verisi kaybı | ORTA | ÇOK YÜKSEK | Yedekleme öncelikli, dikkatli yazma |
| R4 | AMBE+2 patent ihlali | DÜŞÜK | ÇOK YÜKSEK | Yalnızca SCT3258 donanımı kullan |
| R5 | RAM yetersizliği | ORTA | YÜKSEK | Bellek optimizasyonu, modüler yükleme |
| R6 | Cihaz brick | ORTA | YÜKSEK | Bootloader koruma, kurtarma modu |
| R7 | Orijinal FW geri dönüş başarısız | DÜŞÜK | ÇOK YÜKSEK | Üretici bootloader koruma |
| R8 | LCD kontrolcü yanlış tahmin | ORTA | ORTA | Teardown ile doğrulama |
| R9 | RF kalibrasyon uyumsuzluğu | ORTA | YÜKSEK | Kalibrasyon verisi yedekleme |
| R10 | Topluluk desteği yetersiz | ORTA | ORTA | RadioFoundry, OpenRTX işbirliği |

---

## 6. Önerilen Geliştirme Öncelikleri

```
Faz 1: ████████░░ Donanım Doğrulama (teardown + SWD)
Faz 2: ██████░░░░ Temel Sistem (HAL + UI + Tuş)
Faz 3: ████████░░ Analog FM (AT1846S RX/TX)
Faz 4: ██████████ DMR (SCT3258 - RadioFoundry ile)
Faz 5: ████░░░░░░ GPS + APRS
Faz 6: ██████░░░░ M17 Protocol
Faz 7: ████████░░ CPS Yazılımı (AT-D878 Studio)
```

**Kritik Yol:** Faz 1 → Faz 2 → Faz 3 → Faz 4

Faz 1'deki donanım doğrulaması olmadan diğer fazlar başlayamaz.

---

## 7. Sonuç ve Öneriler

### Uygulanabilir (Kısa-Orta Vade):
1. ✅ Temel sistem (boot, HAL, UI, tuş takımı)
2. ✅ Analog FM TX/RX
3. ✅ GPS ve konum gösterimi
4. ✅ APRS (önce TX, sonra RX)
5. ✅ CPS yazılımı (AT-D878 Studio)
6. ✅ FM broadcast alıcısı

### Uygulanabilir (Uzun Vade, Koşullu):
7. 🟡 DMR Tier II (SCT3258 HPI çözülürse)
8. 🟡 M17 Protocol (baseband erişimi sağlanırsa)
9. 🟡 Bluetooth ses ve kontrol

### Şu An İçin Uygulanamaz:
10. 🔴 P25 (vocoder patent + modülasyon uyumsuzluğu)
11. 🔴 NXDN (vocoder patent)
12. 🔴 YSF (vocoder patent)
13. 🔴 D-STAR (vocoder patent)
14. 🔴 DMR yazılımsal bypass (kaynak ve patent kısıtlamaları)

### Acil Eylem Gerektiren Konular:
1. **Fiziksel teardown yapılmalı** - bileşen modellerinin doğrulanması şart
2. **USB protokolü yakalanmalı** - Wireshark + USBPcap ile CPS trafiği
3. **RadioFoundry ile iletişim** - SCT3258 araştırma bulguları paylaşımı
4. **Orijinal codeplug yedeği alınmalı** - herhangi bir deney öncesi

---

*Bu fizibilite raporu mevcut açık kaynak araştırmaları ve üretici
dokümanlarına dayanmaktadır. Fiziksel donanım doğrulaması sonrası
güncellenecektir.*
