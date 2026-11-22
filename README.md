# AnyTone AT-D878 Open Firmware & AT-D878 Studio Suite

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE)
[![MCU: GD32F303VGT6](https://img.shields.io/badge/MCU-GD32F303%20120MHz-green.svg)](https://www.gigadevice.com)
[![Architecture: ARM Cortex-M4](https://img.shields.io/badge/Arch-ARM%20Cortex--M4-orange.svg)]()
[![Desktop: PySide6 + Unicorn](https://img.shields.io/badge/Desktop-PySide6%20%7C%20Unicorn-purple.svg)]()
[![Radio: AT-D878UVII Plus](https://img.shields.io/badge/Radio-AT--D878UVII%20Plus%20V4.00-red.svg)]()

> **An independent, modular, open-source firmware and modern desktop programming studio (CPS & Multi-Flasher) for the AnyTone AT-D878 series handheld DMR/FM transceiver.**
>
> **Project Initiator:** TA5THK (Fatih ATCI)

---

## 🌟 Overview & Mission

The **AnyTone AT-D878UVII Plus** is one of the most popular dual-band DMR/FM handheld transceivers in amateur radio. However, the factory software ecosystem is closed-source, limited to traditional Windows CPS tools, and lacks extensible community capabilities.

This project delivers:
1. **Open Source Firmware (`firmware/`):** A bare-metal, register-level C implementation for the GigaDevice **GD32F303VGT6** (ARM Cortex-M4 @ 120 MHz), driving the Auctus 1846N transceiver, Sicomm SCT3258 DMR baseband, TLV320AIC3204 audio codec, and ST7735 160x128 color LCD.
2. **AT-D878 Studio (`desktop/at_d878_studio/`):** A modern, dark-themed desktop suite combining a full codeplug editor, 500,000 DMR contact database manager, APRS/GPS roaming configuration, multi-subsystem flasher, and an **interactive ARM Cortex-M4 Unicorn Engine simulator** that emulates firmware output before touching actual hardware.

---

## 🛡️ Safety & Non-Destructive Flashing Guarantee

Developing custom firmware carries risks if not handled with absolute precision. This project is built on **zero-brick principles**:
- **Protected Bootloader:** The factory ROM bootloader at `0x08000000 - 0x08003FFF` (16 KB) is **never overwritten**. All custom firmware is linked exclusively at the application base address: `0x08004000`.
- **Factory Reversibility:** You can flash back to official stock firmware (`.CDD`) at any time using standard update modes.
- **Dry-Run by Default:** The flasher and emulator default to non-destructive simulation modes with full hex protocol logging.
- **Hardware-in-the-Loop Simulation:** You can load and verify firmware binaries inside the built-in Cortex-M4 emulator without connecting the radio.

---

## 🔬 Reverse-Engineered Hardware Architecture

Extracted from official firmware V4.00 (`D878UVII_V4.00_20250725.CDD`) and hardware codeplug analysis:

| Subsystem | Component | Bus / Pinout | Details |
|---|---|---|---|
| **Main MCU** | GigaDevice GD32F303VGT6 | ARM Cortex-M4 | 120 MHz, 1 MB Flash, 96 KB SRAM |
| **RF Transceiver** | Auctus 1846N | SPI (`PB1`, `PB2`) | 134-174 MHz VHF + 400-520 MHz UHF |
| **Baseband / Vocoder** | Sicomm/CML SCT3258TD | 8-bit HPI (`PE0`..`PE7`) | Hardware AMBE+2 DMR vocoder & framing |
| **Color LCD** | 1.77" TFT (160x128 RGB565) | SPI (`PC3`, `PC4`, `PC5`, `PC10`) | ST7735 / ILI9163 controller |
| **Audio Codec** | TI TLV320AIC3204 | I2S / I2C (`PA7` Audio Mute) | DAC/ADC & amplifier control |
| **LinkBoard Sub-PCB** | BHT V2.00 (H/M) | Internal UART | 1200 Baud AFSK APRS RX & Bluetooth |
| **External Flash** | SPI NAND/NOR (256 MB) | SPI Flash bus | 3.7 MB Icon/Font pack + 500k Contacts |

### Verified Hardware Bootloader Modes
- **Main MCU Firmware Update:** Hold `PTT + PF3 (Top Green Key)` while powering ON. (Red LED blinks, screen remains off).
- **Icon / Font Pack Update:** Hold `PTT + PF2 (Bottom Side Key)` while powering ON. (Screen displays `UPDATE MODE`).
- **SCT3258 DMR Baseband Update:** Hold `# + PF3` while powering ON. (Screen displays `WARNING This is Boot Mode for Sct!!!`).
- **LinkBoard APRS/BT Update:** Hold `PF3 + PF2` while powering ON. (Screen displays `UPDATE MODE FOR LinkBoard`).
- **Factory MCU Reset:** Hold `PTT + PF1 (Top Side Key)` while powering ON.

---

## 💻 AT-D878 Studio Features

The desktop application includes 9 dedicated modules:
1. **📊 Device Dashboard:** Live hardware telemetry, GD32 MCU memory map, and connected radio model display.
2. **📻 Channels & VFO Matrix:** Dynamic search, zone management, colored mode badges (`[FM]`, `[DMR]`), and CSV export/import.
3. **👥 DMR Digital Contacts (500k):** High-capacity contact book, RadioID.net JSON/CSV parser, search filter, and preloaded BrandMeister Turkey & emergency groups (112, 911, 286).
4. **📡 APRS & GPS Roaming:** 1200 Baud AFSK (144.800 MHz EU/TR, 144.390 MHz US), SmartBeaconing, DMR APRS gateway, and live packet monitor.
5. **🎛️ Optional Settings & Keys:** Programmable functions for PF1, PF2, PF3 (Short/Long press), mic gains (1-5), and Dim Shut mode.
6. **📺 Live Radio LCD Simulator & Firmware Emulator:** Real-time 160x128 color LCD simulation powered by **Unicorn Engine**, allowing firmware execution tracing, register inspection, and interactive PTT/VFO testing.
7. **🎨 Boot Screen Manager:** 40+ pre-built AnyTone BMP themes and 16-bit RGB565 custom image validator.
8. **⚡ Multi-Target Flasher:** 4 independent firmware targets (MCU, Icon SPI Flash, SCT3258, LinkBoard) with automatic button combo guides and dry-run safety.
9. **📖 Hardware Documentation:** Built-in technical documentation reader.

---

## 🛠️ Building & Running

### Prerequisites
- **Firmware Compilation:**
  - `arm-none-eabi-gcc` (ARM GNU Toolchain 12.x or later)
- **Desktop Studio:**
  - Python 3.10+ with `PySide6`, `pyserial`, `pyyaml`, `unicorn`

### Building the Firmware
```bash
cd firmware
# Compile and link open firmware
arm-none-eabi-gcc -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -O2 -Wall -Wextra \
  -ffunction-sections -fdata-sections --specs=nano.specs --specs=nosys.specs \
  -Icore/include -Ihal/include -Idrivers/include -Iradio/include -Iui/include \
  "-Tcore/gd32f303vgt6.ld" "-Wl,--gc-sections" -nostartfiles \
  -o build/at_d878_fw.elf \
  core/src/startup_gd32f303.c core/src/main.c hal/src/hal_gpio.c hal/src/hal_spi.c \
  hal/src/hal_i2c.c drivers/src/driver_display.c drivers/src/driver_at1846s.c \
  drivers/src/driver_audio.c drivers/src/driver_keyboard.c radio/src/radio_core.c \
  ui/src/ui_screen.c

# Extract binary and package to .CDD / .CDI / .spi
arm-none-eabi-objcopy -O binary build/at_d878_fw.elf build/at_d878_fw.bin
python ../tools/firmware_analyzer/make_firmware_pkg.py build/at_d878_fw.bin -o ../output_firmware
```

### Running AT-D878 Studio
```bash
cd desktop/at_d878_studio
python -m venv .venv
.venv\Scripts\activate
pip install -r requirements.txt
python main.py
```
Or double-click `AT-D878 Studio.lnk` on Windows.

---

## 🤝 Contributing

We warmly welcome contributions from amateur radio operators, embedded developers, and reverse engineers worldwide!
- Join development discussions and submit issues.
- Please review [CONTRIBUTING.md](CONTRIBUTING.md) before submitting Pull Requests.

---

## 📜 License

This project is licensed under the **GNU General Public License v3.0 (GPLv3)**. See [LICENSE](LICENSE) for details.

*AnyTone is a trademark of Qixiang Electron Science & Technology Co., Ltd. This independent research project is not affiliated with or endorsed by AnyTone or Qixiang.*
