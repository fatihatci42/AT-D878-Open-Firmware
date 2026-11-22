#!/usr/bin/env python3
"""
AnyTone Codeplug (.rdt / binary) Inspector & Parser Tool
Orijinal AnyTone yedeklerini ve kod fişlerini inceler, kanal ve ayarları ayrıştırır.
"""

import sys
import struct
import argparse
import os

class CodeplugParser:
    def __init__(self, filepath):
        self.filepath = filepath
        self.data = None

    def load(self):
        if not os.path.exists(self.filepath):
            raise FileNotFoundError(f"Dosya bulunamadı: {self.filepath}")
        with open(self.filepath, "rb") as f:
            self.data = f.read()
        print(f"[+] Codeplug yüklendi: {self.filepath} ({len(self.data)} byte / {len(self.data)/1024:.1f} KB)")

    def inspect_header(self):
        if len(self.data) < 64:
            print("[-] Hata: Dosya boyutu çok küçük.")
            return

        header = self.data[:64]
        print("\n--- CODEPLUG BAŞLIK ANALİZİ ---")
        print("HEX Dump:")
        for i in range(0, 64, 16):
            chunk = header[i:i+16]
            hex_part = ' '.join(f'{b:02X}' for b in chunk)
            ascii_part = ''.join(chr(b) if 32 <= b <= 126 else '.' for b in chunk)
            print(f"  {i:04X}:  {hex_part:<48}  |{ascii_part}|")

def main():
    parser = argparse.ArgumentParser(description="AnyTone Codeplug Inspector")
    parser.add_argument("file", help="AnyTone .rdt veya .bin dosya yolu")
    args = parser.parse_args()

    cp = CodeplugParser(args.file)
    cp.load()
    cp.inspect_header()

if __name__ == "__main__":
    main()
