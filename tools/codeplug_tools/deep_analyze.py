#!/usr/bin/env python3
"""
AnyTone AT-D878UVII Codeplug Deep Analyzer
Orijinal yedekten cihaz modeli, kanal sayısı, bölgeler ve radyo parametrelerini çıkarır.
"""

import sys
import struct
import os

def analyze_codeplug(filepath):
    with open(filepath, "rb") as f:
        data = f.read()

    print(f"==================================================")
    print(f"CODEPLUG DETAYLI ANALİZ RAPORU")
    print(f"Dosya: {os.path.basename(filepath)}")
    print(f"Toplam Boyut: {len(data)} byte ({len(data)/1024:.2f} KB)")
    print(f"==================================================")

    # 1. Başlık ve Model Kimliği
    magic_ver = data[0:8].decode('ascii', errors='replace').strip('\x00')
    model_str = data[0x09:0x13].decode('ascii', errors='replace').strip('\x00')
    fw_ver = data[0x13:0x18].decode('ascii', errors='replace').strip('\x00')

    print(f"[+] Başlık Sürüm Kodu : {magic_ver}")
    print(f"[+] Doğrulanan Model  : {model_str}")
    print(f"[+] CPS/FW Belirteci  : {fw_ver}")

    # 2. Metin Arama (Callsign, DMR ID, Kanal İsimleri vb.)
    strings_found = []
    current_str = []
    for idx, b in enumerate(data):
        if 32 <= b <= 126:
            current_str.append(chr(b))
        else:
            if len(current_str) >= 4:
                s = "".join(current_str)
                strings_found.append((idx - len(current_str), s))
            current_str = []

    print(f"\n[+] Tespit Edilen Yapılandırma Metinleri (Örnekler):")
    unique_samples = []
    for offset, s in strings_found:
        if any(keyword in s for keyword in ["D878", "V10", "F1234", "CH", "Zone", "TG", "DMR", "TA"]):
            unique_samples.append((offset, s))
    
    for offset, s in unique_samples[:15]:
        print(f"    0x{offset:06X}: {s}")

if __name__ == "__main__":
    path = r"C:\Users\Fatih ATCI\Desktop\Anytone\AT-D878-Open-Firmware\hardware\backup_stock_codeplug.rdt"
    analyze_codeplug(path)
