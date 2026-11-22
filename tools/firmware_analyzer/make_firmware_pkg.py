#!/usr/bin/env python3
"""
AnyTone AT-D878 Open Firmware Package Builder
Derlenen ham ARM binary dosyasını (.bin / .elf), resmi AnyTone güncelleme
araçlarının ve açık kaynak flasher'ımızın tanıyacağı .CDD / .CDI / .spi paketine dönüştürür.
"""

import sys
import os
import struct
import argparse
import zlib

TARGET_MODEL = "D878UV2"
APP_BASE_ADDR = 0x08004000
MAX_FW_SIZE = 960 * 1024  # Maksimum 960 KB

def build_package(bin_path, output_dir, version_str="V4.00_OPEN"):
    if not os.path.exists(bin_path):
        raise FileNotFoundError(f"Binary bulunamadı: {bin_path}")

    with open(bin_path, "rb") as f:
        raw_bin = f.read()

    size = len(raw_bin)
    print(f"[*] Kaynak Binary Yüklendi: {bin_path} ({size} byte / {size/1024:.2f} KB)")

    if size > MAX_FW_SIZE:
        raise ValueError(f"HATA: Firmware boyutu izin verilen {MAX_FW_SIZE} byte sınırını aşıyor!")

    # 1. ARM Cortex-M Vektör Tablosu Sağlamlık Kontrolü
    sp, reset = struct.unpack("<II", raw_bin[:8])
    print(f"[*] Initial SP: 0x{sp:08X}, Reset Handler: 0x{reset:08X}")
    if (sp & 0xFFFE0000) != 0x20000000 or (reset & 0xFFF00000) != 0x08000000:
        print("[!] UYARI: Vektör tablosu GD32 SRAM / Flash adres alanına tam uymuyor olabilir.")

    os.makedirs(output_dir, exist_ok=True)
    base_name = f"D878UVII_{version_str}"
    cdd_file = os.path.join(output_dir, f"{base_name}.CDD")
    cdi_file = os.path.join(output_dir, f"{base_name}.CDI")
    spi_file = os.path.join(output_dir, f"{base_name}.spi")

    # 2. CDD Dosyası (Doğrudan ham binary payload)
    with open(cdd_file, "wb") as f:
        f.write(raw_bin)
    print(f"[+] Üretildi: {cdd_file} ({size} byte)")

    # 3. CDI Dosyası (Metadata / İndeks)
    # CDI formatı: 278 byte uzunluğunda açıklayıcı başlık
    cdi_payload = bytearray(278)
    name_entry = b"D878UV_1G.bin"
    cdi_payload[:len(name_entry)] = name_entry
    # Offset 0xF0 civarına boyut ve imza parametreleri
    struct.pack_into("<I", cdi_payload, 0xF0, size)
    struct.pack_into("<I", cdi_payload, 0xF4, APP_BASE_ADDR)
    with open(cdi_file, "wb") as f:
        f.write(cdi_payload)
    print(f"[+] Üretildi: {cdi_file} (278 byte)")

    # 4. SPI Tanımlayıcı Dosyası (51 Byte model imzası)
    # Format: "      @6      D878UV2 V100D878UV2         V100     "
    spi_payload = b"      @6      D878UV2 V100D878UV2         V100     "
    with open(spi_file, "wb") as f:
        f.write(spi_payload[:51])
    print(f"[+] Üretildi: {spi_file} (51 byte)")

    print("\n[✓] Firmware güncelleme paketi başarıyla oluşturuldu!")
    print(f"    Paket Dizini: {output_dir}")

def main():
    parser = argparse.ArgumentParser(description="AnyTone Open Firmware Package Builder")
    parser.add_argument("binary", help="Derlenen at_d878_fw.bin dosyasının yolu")
    parser.add_argument("-o", "--outdir", default="./output_package", help="Çıktı klasörü")
    parser.add_argument("-v", "--version", default="V4.00_OPEN", help="Paket sürüm etiketi")
    args = parser.parse_args()

    build_package(args.binary, args.outdir, args.version)

if __name__ == "__main__":
    if len(sys.argv) > 1:
        main()
    else:
        print("Kullanım: python make_firmware_pkg.py <firmware.bin> [-o cikti_klasoru]")
