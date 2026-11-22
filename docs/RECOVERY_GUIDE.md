# AT-D878 Open Firmware - Kurtarma Kılavuzu

**Durum:** Planlama Aşaması  
**ÖNEMLİ:** Bu kılavuz henüz donanım üzerinde doğrulanmamıştır.

---

## 1. Kurtarma Felsefesi

> **Altın Kural:** Hiçbir deney, cihazı kurtarılamaz duruma düşürmemelidir.

### 1.1. Kurtarma Katmanları

```
Katman 1: Orijinal üretici bootloader (DOKUNMA)
    │
    ▼
Katman 2: Orijinal firmware yedeği (harici saklama)
    │
    ▼
Katman 3: Orijinal codeplug yedeği (harici saklama)
    │
    ▼
Katman 4: Open firmware kurtarma modu
    │
    ▼
Katman 5: SWD/JTAG erişimi (son çare)
```

---

## 2. Deney Öncesi Yedekleme Prosedürü

### 2.1. Codeplug Yedeği
```
1. Orijinal CPS yazılımını kullanarak codeplug'ı oku
2. .rdt dosyası olarak kaydet
3. anytone-flash-tools ile ikinci bir yedek al
4. Her iki yedeği güvenli konumda sakla
5. MD5/SHA256 hash değerlerini kaydet
```

### 2.2. Firmware Yedeği (Araştırılıyor)
```
⚠️ DİKKAT: Firmware okuma yöntemi henüz doğrulanmamıştır.
Olası yöntemler:
- SWD üzerinden flash dump (okuma koruması sorunlu olabilir)
- USB protokolü üzerinden firmware okuma (araştırılıyor)
- Firmware güncelleme dosyasından (.spi) orijinal binary çıkarma
```

### 2.3. Kalibrasyon Verisi
```
⚠️ KRİTİK: Kalibrasyon verisi her cihaza özgüdür.
- RF güç kalibrasyonu
- Frekans doğruluğu kalibrasyonu
- ADC kalibrasyonu
Bu verilerin yeri ve formatı araştırılmaktadır.
```

---

## 3. Kurtarma Yöntemleri

### 3.1. Üretici Bootloader ile Kurtarma
En güvenli yöntem. Üretici bootloader'ı korunduğu sürece:
1. Cihazı DFU/programlama modunda başlat
2. Orijinal CPS veya firmware güncelleme aracı ile orijinal .spi dosyasını yükle
3. Codeplug'ı geri yükle

### 3.2. SWD ile Kurtarma (Son Çare)
1. PCB üzerinde SWD pinlerini bul (SWDIO, SWCLK, GND, 3.3V)
2. ST-Link V2 veya J-Link bağla
3. OpenOCD ile bağlantı kur
4. Flash'ı orijinal yedekten geri yükle

**⚠️ UYARI:** GD32F303'te flash okuma koruması (RDP) aktifse, SWD ile
okuma mümkün olmayabilir. Bu durumda "mass erase" tek seçenek olur ve
tüm kalibrasyon verisi kaybolur.

---

## 4. Önleme Kuralları

1. **ASLA** bootloader bölgesine yazma
2. **ASLA** kalibrasyon bölgesini silme veya üzerine yazma
3. **HER ZAMAN** deney öncesi yedek al
4. **HER ZAMAN** önce simülatör/emülatörde test et
5. **HER ZAMAN** dummy load ile RF testi yap
6. **HER ZAMAN** deney logları tut

---

*Bu kılavuz donanım doğrulaması sonrası güncellenecektir.*
