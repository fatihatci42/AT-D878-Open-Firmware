#!/usr/bin/env python3
"""
AnyTone AT-D878UVII V4.00 Firmware Deep Extractor
CDD binary dosyasından donanım register erişimlerini, çevre birimlerini ve konfigürasyon dizgilerini çıkarır.
"""

import sys
import os
import struct

def extract_firmware_info(cdd_path):
    with open(cdd_path, "rb") as f:
        data = f.read()

    base_addr = 0x08004000
    print("=" * 60)
    print("ANYTONE AT-D878UVII V4.00 FIRMWARE DERİN ANALİZİ")
    print(f"Kaynak Dosya: {os.path.basename(cdd_path)}")
    print(f"Boyut: {len(data)} byte ({len(data)/1024:.2f} KB)")
    print(f"Yükleme Taban Adresi: 0x{base_addr:08X}")
    print("=" * 60)

    # 1. Vektör Tablosu Analizi
    sp = struct.unpack("<I", data[0:4])[0]
    reset = struct.unpack("<I", data[4:8])[0]
    print(f"\n[+] Vektör Tablosu:")
    print(f"    - Initial Stack Pointer (MSP) : 0x{sp:08X}")
    print(f"    - Reset Handler               : 0x{reset:08X} (Dosya Ofseti: 0x{reset - 1 - base_addr:06X})")

    # 2. İlginç Metinler (Firmware içinde gömülü strings)
    print(f"\n[+] Firmware İçi Tanımlayıcı Dizgiler (Örnekler):")
    min_len = 5
    cur_str = []
    strings = []
    for idx, b in enumerate(data):
        if 32 <= b <= 126:
            cur_str.append(chr(b))
        else:
            if len(cur_str) >= min_len:
                s = "".join(cur_str)
                strings.append((base_addr + idx - len(cur_str), s))
            cur_str = []

    keywords = ["D878", "V4.0", "AT1846", "SCT", "AIC", "GD32", "STM32", "Init", "UART", "SPI", "I2C"]
    found_keywords = {}
    for addr, s in strings:
        for kw in keywords:
            if kw.lower() in s.lower():
                if kw not in found_keywords:
                    found_keywords[kw] = []
                if len(found_keywords[kw]) < 3:
                    found_keywords[kw].append((addr, s))

    for kw, hits in found_keywords.items():
        print(f"  * Anahtar Kelime [{kw}]:")
        for addr, s in hits:
            print(f"      0x{addr:08X}: {s}")

    # 3. GD32F303 Çevre Birim Register Adres Taraması
    # AHB/APB Base Adresleri:
    # GPIOA: 0x40010800, GPIOB: 0x40010C00, GPIOC: 0x40011000, GPIOD: 0x40011400, GPIOE: 0x40011800
    # SPI0: 0x40013000, SPI1: 0x40003800, SPI2: 0x40003C00
    # I2C0: 0x40005400, I2C1: 0x40005800
    # USART0: 0x40013800, USART1: 0x40004400, USART2: 0x40004800, UART3: 0x40004C00, UART4: 0x40005000
    peripherals = {
        "GPIOA": 0x40010800, "GPIOB": 0x40010C00, "GPIOC": 0x40011000, "GPIOD": 0x40011400, "GPIOE": 0x40011800,
        "SPI0": 0x40013000, "SPI1": 0x40003800, "SPI2": 0x40003C00,
        "I2C0": 0x40005400, "I2C1": 0x40005800,
        "USART0": 0x40013800, "USART1": 0x40004400, "USART2": 0x40004800, "UART3": 0x40004C00, "UART4": 0x40005000,
        "RCU": 0x40021000, "DMA0": 0x40020000, "DMA1": 0x40020400, "EXMC": 0xA0000000
    }

    print(f"\n[+] GD32F303 Donanım Çevre Birimi Referans Taraması:")
    for name, p_addr in peripherals.items():
        pattern = struct.pack("<I", p_addr)
        count = data.count(pattern)
        if count > 0:
            print(f"    - {name:<8} (0x{p_addr:08X}) : {count} kez doğrudan referans verildi!")

if __name__ == "__main__":
    cdd = r"C:\Users\Fatih ATCI\Desktop\Anytone\AT-D878-Open-Firmware\tools\firmware_analyzer\D878UVII_V4.00 FW\D878UVII_V4.00_20250725.CDD"
    extract_firmware_info(cdd)
