#!/usr/bin/env python3
"""
AT-D878 Open Firmware Automated Build System
ARM GCC Toolchain kullanarak tüm C ve Assembly dosyalarını derler,
bağlar (linking), .bin oluşturur ve .CDD/.CDI paketini hazır eder.
"""

import os
import sys
import subprocess
import shutil

PROJECT_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
BUILD_DIR = os.path.join(PROJECT_ROOT, "build")
OUTPUT_DIR = os.path.join(PROJECT_ROOT, "output_firmware")

# Toolchain yolu
TOOLCHAIN_BIN = r"C:\Program Files\Arm\GNU Toolchain mingw-w64-x86_64-arm-none-eabi\bin"
CC = os.path.join(TOOLCHAIN_BIN, "arm-none-eabi-gcc.exe")
OBJCOPY = os.path.join(TOOLCHAIN_BIN, "arm-none-eabi-objcopy.exe")
SIZE = os.path.join(TOOLCHAIN_BIN, "arm-none-eabi-size.exe")

if not os.path.exists(CC):
    # PATH'te ara
    which_cc = shutil.which("arm-none-eabi-gcc")
    if which_cc:
        CC = which_cc
        OBJCOPY = shutil.which("arm-none-eabi-objcopy")
        SIZE = shutil.which("arm-none-eabi-size")
    else:
        print(f"[!] HATA: arm-none-eabi-gcc bulunamadı: {CC}")
        sys.exit(1)

# Include Dizinleri
INCLUDES = [
    os.path.join(PROJECT_ROOT, "firmware", "core", "include"),
    os.path.join(PROJECT_ROOT, "firmware", "hal", "include"),
    os.path.join(PROJECT_ROOT, "firmware", "drivers", "include"),
    os.path.join(PROJECT_ROOT, "firmware", "radio", "include"),
    os.path.join(PROJECT_ROOT, "firmware", "ui", "include"),
    os.path.join(PROJECT_ROOT, "firmware", "config", "include"),
    os.path.join(PROJECT_ROOT, "firmware", "audio", "include"),
    os.path.join(PROJECT_ROOT, "firmware", "gps", "include"),
]

# Kaynak Dosyaları
SOURCES = [
    os.path.join(PROJECT_ROOT, "firmware", "core", "src", "startup_gd32f303.c"),
    os.path.join(PROJECT_ROOT, "firmware", "core", "src", "main.c"),
    os.path.join(PROJECT_ROOT, "firmware", "hal", "src", "hal_gpio.c"),
    os.path.join(PROJECT_ROOT, "firmware", "hal", "src", "hal_spi.c"),
    os.path.join(PROJECT_ROOT, "firmware", "hal", "src", "hal_i2c.c"),
    os.path.join(PROJECT_ROOT, "firmware", "drivers", "src", "driver_display.c"),
    os.path.join(PROJECT_ROOT, "firmware", "drivers", "src", "driver_at1846s.c"),
    os.path.join(PROJECT_ROOT, "firmware", "drivers", "src", "driver_audio.c"),
    os.path.join(PROJECT_ROOT, "firmware", "drivers", "src", "driver_keyboard.c"),
    os.path.join(PROJECT_ROOT, "firmware", "radio", "src", "radio_core.c"),
    os.path.join(PROJECT_ROOT, "firmware", "ui", "src", "ui_screen.c"),
]

LINKER_SCRIPT = os.path.join(PROJECT_ROOT, "firmware", "core", "gd32f303vgt6.ld")

CFLAGS = [
    "-mcpu=cortex-m4",
    "-mthumb",
    "-mfpu=fpv4-sp-d16",
    "-mfloat-abi=hard",
    "-Og",
    "-g3",
    "-Wall",
    "-fdata-sections",
    "-ffunction-sections",
    "-fno-common",
    "-fno-exceptions",
    "-DGD32F303",
    "-DGD32F303VGT6",
]

LDFLAGS = [
    "-mcpu=cortex-m4",
    "-mthumb",
    "-mfpu=fpv4-sp-d16",
    "-mfloat-abi=hard",
    f"-T{LINKER_SCRIPT}",
    "-Wl,--gc-sections",
    "--specs=nano.specs",
    "--specs=nosys.specs",
    "-lm",
    "-lc",
]

def build():
    print("=" * 60)
    print("AT-D878 OPEN FIRMWARE BUILD ENGINE")
    print("=" * 60)
    os.makedirs(BUILD_DIR, exist_ok=True)

    # 1. Her C dosyasını derle (.o)
    objects = []
    inc_flags = [f"-I{inc}" for inc in INCLUDES]

    for src in SOURCES:
        if not os.path.exists(src):
            print(f"[!] Hata: Dosya eksik: {src}")
            return False

        obj_name = os.path.splitext(os.path.basename(src))[0] + ".o"
        obj_path = os.path.join(BUILD_DIR, obj_name)
        objects.append(obj_path)

        cmd = [CC] + CFLAGS + inc_flags + ["-c", src, "-o", obj_path]
        print(f"[*] Derleniyor: {os.path.basename(src)}...")
        res = subprocess.run(cmd)
        if res.returncode != 0:
            print(f"[!] Derleme Hatası: {src}")
            return False

    # 2. Bağla (Linking -> ELF)
    elf_path = os.path.join(BUILD_DIR, "at_d878_fw.elf")
    map_path = os.path.join(BUILD_DIR, "at_d878_fw.map")
    link_cmd = [CC] + LDFLAGS + [f"-Wl,-Map={map_path}"] + objects + ["-o", elf_path]

    print("\n[*] Bağlanıyor (Linking ELF)...")
    res = subprocess.run(link_cmd)
    if res.returncode != 0:
        print("[!] Link Hatası!")
        return False

    # 3. Boyut Raporu
    print("\n[*] Bellek Kullanım Raporu:")
    subprocess.run([SIZE, elf_path])

    # 4. Binary (.bin) Çıkart
    bin_path = os.path.join(BUILD_DIR, "at_d878_fw.bin")
    subprocess.run([OBJCOPY, "-O", "binary", elf_path, bin_path])
    print(f"[+] Ham Binary Üretildi: {bin_path} ({os.path.getsize(bin_path)} byte)")

    # 5. AnyTone .CDD / .CDI / .spi Paketini Üret
    print("\n[*] USB Firmware Güncelleme Paketi Paketleniyor...")
    packager_script = os.path.join(PROJECT_ROOT, "tools", "firmware_analyzer", "make_firmware_pkg.py")
    pkg_cmd = [sys.executable, packager_script, bin_path, "-o", OUTPUT_DIR, "-v", "V4.00_OPEN_ALPHA"]
    subprocess.run(pkg_cmd)

    return True

if __name__ == "__main__":
    success = build()
    if not success:
        sys.exit(1)
