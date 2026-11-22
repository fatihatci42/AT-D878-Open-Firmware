# AT-D878 Open Firmware - Derleme Kılavuzu

**Platform:** Windows (öncelikli), Linux, macOS  
**Durum:** Planlama Aşaması

---

## 1. Gerekli Araçlar

### 1.1. Toolchain
- **ARM GCC Toolchain:** `arm-none-eabi-gcc` (12.x veya üzeri)
  - İndir: https://developer.arm.com/downloads/-/gnu-rm
- **CMake:** 3.20 veya üzeri
- **Ninja:** Hızlı build backend
- **Python 3.10+:** Script'ler ve CPS geliştirme için

### 1.2. Debug Araçları
- **OpenOCD:** GD32F303 SWD debug desteği
- **ST-Link V2** veya uyumlu SWD probe
- **PuTTY / Tera Term:** Seri port debug

### 1.3. Analiz Araçları
- **Wireshark + USBPcap:** USB protokol analizi
- **Logic Analyzer:** SPI/I2C/UART sinyal analizi (Saleae veya uyumlu)
- **Sigrok / PulseView:** Açık kaynak logic analyzer yazılımı

### 1.4. RF Test Ekipmanları
- **Dummy Load:** 50Ω, min. 10W
- **RF Güç Metre:** Çıkış gücü doğrulama
- **Frekans Sayıcı:** Frekans doğruluğu ölçümü
- **SWR Metre:** Anten uyumu kontrolü

---

## 2. Ortam Kurulumu (Windows)

```powershell
# 1. ARM GCC Toolchain kurulumu
# https://developer.arm.com/downloads/-/gnu-rm adresinden indirin
# PATH'e ekleyin: C:\Program Files (x86)\GNU Arm Embedded Toolchain\bin

# 2. CMake kurulumu
winget install Kitware.CMake

# 3. Ninja kurulumu
winget install Ninja-build.Ninja

# 4. Python kurulumu
winget install Python.Python.3.12

# 5. Git kurulumu (zaten kurulu değilse)
winget install Git.Git

# 6. Doğrulama
arm-none-eabi-gcc --version
cmake --version
ninja --version
python --version
```

---

## 3. Proje Derleme (Planlanan)

```powershell
# Repository klonlama
git clone https://github.com/[owner]/AT-D878-Open-Firmware.git
cd AT-D878-Open-Firmware

# Build dizini oluşturma
mkdir build
cd build

# CMake konfigürasyonu
cmake -G Ninja -DCMAKE_TOOLCHAIN_FILE=../cmake/arm-gcc-toolchain.cmake ..

# Derleme
ninja

# Çıktı: build/firmware.bin, build/firmware.hex, build/firmware.elf
```

---

## 4. CPS Geliştirme Ortamı (AT-D878 Studio)

```powershell
# Sanal ortam oluşturma
cd desktop/at_d878_studio
python -m venv venv
venv\Scripts\activate

# Bağımlılıklar
pip install PySide6 pyserial pyyaml

# Çalıştırma
python main.py
```

---

## 5. Güvenlik Kontrol Listesi

Derleme ve test öncesi her zaman kontrol edin:

- [ ] Orijinal firmware yedeklendi mi?
- [ ] Orijinal codeplug yedeklendi mi?
- [ ] Dummy load bağlı mı? (RF testleri için)
- [ ] Pil şarj seviyesi yeterli mi?
- [ ] Debug seri port bağlı mı?
- [ ] Kurtarma yöntemi doğrulandı mı?
