#!/usr/bin/env python3
"""
SCT3258 Bootloader Extractor Tool for AnyTone AT-D878 / D868 / D578
Extracts the 2048-byte (1024 16-bit words) SCT3258 baseband bootloader block
from official vendor firmware (.CDD or .bin) using verified SHA-256 signature.

Reference: RadioFoundry (https://github.com/RadioFoundry/RadioFoundry)
Target SHA-256: 743515b9f7cd90737c1fff29b652b7558bc5a56487ec38df4fdea8877e7a619a
"""

import sys
import os
import hashlib
import argparse

SCT3258_BOOT_SHA256 = "743515b9f7cd90737c1fff29b652b7558bc5a56487ec38df4fdea8877e7a619a"
BLOCK_SIZE = 2048  # 1024 16-bit words

def extract_boot_block(input_path, output_bin=None, output_header=None):
    if not os.path.isfile(input_path):
        print(f"[-] Hata: Dosya bulunamadı: {input_path}")
        return False

    with open(input_path, "rb") as f:
        data = f.read()

    print(f"[*] '{input_path}' taranıyor ({len(data)} byte)...")
    found_offset = -1

    for i in range(len(data) - BLOCK_SIZE):
        chunk = data[i:i + BLOCK_SIZE]
        if hashlib.sha256(chunk).hexdigest() == SCT3258_BOOT_SHA256:
            found_offset = i
            break

    if found_offset == -1:
        print("[-] SCT3258 boot bloğu belirtilen SHA-256 ile bulunamadı!")
        return False

    boot_bytes = data[found_offset:found_offset + BLOCK_SIZE]
    print(f"[+] SCT3258 Boot Bloğu Bulundu! Ofset: 0x{found_offset:06X} (Boyut: {len(boot_bytes)} byte)")
    print(f"[+] SHA-256 Doğrulandı: {SCT3258_BOOT_SHA256}")

    # Binary çıktı
    if output_bin:
        with open(output_bin, "wb") as f:
            f.write(boot_bytes)
        print(f"[✓] Kaydedildi: {output_bin}")

    # C Header çıktı
    if output_header:
        with open(output_header, "w", encoding="utf-8") as f:
            f.write("/**\n")
            f.write(" * @file    sct3258_boot_blob.h\n")
            f.write(" * @brief   SCT3258 DMR Baseband Boot Loader Block (1024 16-bit words)\n")
            f.write(" * Extracted from official AnyTone firmware via RadioFoundry SHA-256 signature.\n")
            f.write(" */\n\n")
            f.write("#ifndef SCT3258_BOOT_BLOB_H\n")
            f.write("#define SCT3258_BOOT_BLOB_H\n\n")
            f.write("#include <stdint.h>\n\n")
            f.write(f"#define SCT3258_BOOT_SIZE_WORDS  1024\n")
            f.write(f"#define SCT3258_BOOT_SIZE_BYTES  {BLOCK_SIZE}\n\n")
            f.write("static const uint8_t SCT3258_BOOT_BLOB[2048] = {\n")
            for r in range(0, BLOCK_SIZE, 16):
                line = "    " + ", ".join(f"0x{b:02X}" for b in boot_bytes[r:r+16])
                if r + 16 < BLOCK_SIZE:
                    line += ","
                f.write(line + "\n")
            f.write("};\n\n")
            f.write("#endif /* SCT3258_BOOT_BLOB_H */\n")
        print(f"[✓] C Başlık Dosyası Üretildi: {output_header}")

    return True

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="AnyTone SCT3258 Bootloader Extractor")
    parser.add_argument("input", nargs="?", default="tools/firmware_analyzer/D878UVII PLUS V4.00 official release 250815/D878UVII_V4.00 FW/D878UVII_V4.00_20250725.CDD")
    parser.add_argument("-b", "--bin", default="firmware/drivers/src/sct3258_boot_blob.bin")
    parser.add_argument("-H", "--header", default="firmware/drivers/include/sct3258_boot_blob.h")

    args = parser.parse_args()
    ok = extract_boot_block(args.input, args.bin, args.header)
    sys.exit(0 if ok else 1)
