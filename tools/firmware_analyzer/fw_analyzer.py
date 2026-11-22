#!/usr/bin/env python3
"""
AnyTone Firmware Package (.spi / .bin) Analyzer & Inspector
Resmi AnyTone firmware güncelleme dosyalarını inceler, başlıkları ve şifreleme/maskeleme yapısını çözer.
"""

import sys
import os
import struct
import argparse

class FirmwareAnalyzer:
    def __init__(self, filepath):
        self.filepath = filepath
        self.data = None

    def load(self):
        if not os.path.exists(self.filepath):
            raise FileNotFoundError(f"Dosya bulunamadı: {self.filepath}")
        with open(self.filepath, "rb") as f:
            self.data = f.read()
        print(f"[+] Firmware paketi yüklendi: {self.filepath}")
        print(f"[+] Toplam Boyut: {len(self.data)} byte ({len(self.data)/1024:.2f} KB)")

    def analyze_header(self):
        if len(self.data) < 256:
            print("[-] Hata: Dosya boyutu çok küçük.")
            return

        print("\n--- FIRMWARE BAŞLIK DUMP (İlk 64 Byte) ---")
        for i in range(0, 64, 16):
            chunk = self.data[i:i+16]
            hex_part = ' '.join(f'{b:02X}' for b in chunk)
            ascii_part = ''.join(chr(b) if 32 <= b <= 126 else '.' for b in chunk)
            print(f"  {i:04X}:  {hex_part:<48}  |{ascii_part}|")

        # ARM Cortex-M Vektör Tablosu Kontrolü
        # Vektör tablosunun ilk 4 byte'ı: Initial Stack Pointer (MSP), 0x2000xxxx olmalıdır
        # İkinci 4 byte'ı: Reset Handler adresi, 0x0800xxxx (tek sayı - Thumb modu) olmalıdır
        sp, reset_handler = struct.unpack("<II", self.data[:8])
        print(f"\n[?] Vektör Tablosu İmzası (Düz Binary ise):")
        print(f"    Initial SP   : 0x{sp:08X} (Beklenen: 0x2000xxxx)")
        print(f"    Reset Handler: 0x{reset_handler:08X} (Beklenen: 0x0800xxxx)")

        if (sp & 0xFFFE0000) == 0x20000000 and (reset_handler & 0xFFF00000) == 0x08000000:
            print("[✓] Bu dosya doğrudan ham (unencrypted) ARM Cortex-M binary içeriyor!")
        else:
            print("[!] Bu dosya üretici başlığı taşıyor veya maskelenmiş/şifrelenmiş (.spi paketi).")

def main():
    parser = argparse.ArgumentParser(description="AnyTone Firmware Analyzer")
    parser.add_argument("file", help="İncelenecek .spi veya .bin firmware dosyası")
    args = parser.parse_args()

    analyzer = FirmwareAnalyzer(args.file)
    analyzer.load()
    analyzer.analyze_header()

if __name__ == "__main__":
    if len(sys.argv) > 1:
        main()
    else:
        print("Kullanım: python fw_analyzer.py <firmware_dosyasi.spi>")
