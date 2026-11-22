#!/usr/bin/env python3
"""
AnyTone AT-D878UVII GPIO Configuration & Pinout Tracer
Orijinal V4.00 firmware binary'sindeki CTL0, CTL1, BOP, BC ve ISTAT erişimlerini
analiz ederek donanımdaki tüm pinlerin görevini ortaya çıkarır.
"""

import sys
import struct
import os

BASE_ADDR = 0x08004000

GPIO_PORTS = {
    "GPIOA": 0x40010800,
    "GPIOB": 0x40010C00,
    "GPIOC": 0x40011000,
    "GPIOD": 0x40011400,
    "GPIOE": 0x40011800,
}

# GD32F303 / STM32F103 GPIO Modları
MODE_NAMES = {
    0b0000: "Input Analog",
    0b0100: "Input Floating",
    0b1000: "Input Pull-Up / Pull-Down",
    0b0001: "Output Push-Pull 10MHz",
    0b0010: "Output Push-Pull 2MHz",
    0b0011: "Output Push-Pull 50MHz",
    0b0101: "Output Open-Drain 10MHz",
    0b0110: "Output Open-Drain 2MHz",
    0b0111: "Output Open-Drain 50MHz",
    0b1001: "Alt-Func Push-Pull 10MHz",
    0b1010: "Alt-Func Push-Pull 2MHz",
    0b1011: "Alt-Func Push-Pull 50MHz",
    0b1101: "Alt-Func Open-Drain 10MHz",
    0b1110: "Alt-Func Open-Drain 2MHz",
    0b1111: "Alt-Func Open-Drain 50MHz",
}

def analyze_gpio(cdd_path):
    with open(cdd_path, "rb") as f:
        data = f.read()

    print("=" * 70)
    print("ANYTONE AT-D878UVII PLUS (V4.00) GPIO VE PIN ANALİZİ")
    print("=" * 70)

    # 1. Literal Pool'da Port Adreslerini Bulan Fonksiyonlar
    port_refs = {}
    for name, addr in GPIO_PORTS.items():
        pattern = struct.pack("<I", addr)
        offsets = []
        pos = 0
        while True:
            idx = data.find(pattern, pos)
            if idx == -1:
                break
            offsets.append(idx)
            pos = idx + 4
        port_refs[name] = offsets
        print(f"[+] {name} (0x{addr:08X}) Literal Pool Referans Sayısı: {len(offsets)}")

    # 2. CTL0 / CTL1 Yazmalarını ve Pin Konfigürasyonlarını Tespit Etme
    print("\n[+] GPIO Başlatma Rutinleri ve CTL0/CTL1 Değerleri:")
    # CTL0 ve CTL1 genellikle ardışık 32-bit sabitlerle yüklenir
    # 32-bit sabitler içinde 4-bit per pin: [pin3][pin2][pin1][pin0]
    for name, offsets in port_refs.items():
        print(f"\n--- {name} Pin Analizi ---")
        # Referansların etrafındaki talimatları incele (±64 byte)
        accessed_registers = set()
        for off in offsets[:10]:
            ref_addr = BASE_ADDR + off
            # 64 byte öncesi ve sonrası
            start_scan = max(0, off - 64)
            end_scan = min(len(data), off + 64)
            # STR/LDR offsetlerini ara
            # Thumb STR Rx, [Ry, #imm]
            for i in range(start_scan, end_scan - 2, 2):
                h = struct.unpack("<H", data[i:i+2])[0]
                # STR / LDR formatları (Thumb)
                if (h & 0xF800) == 0x6000: # STR Rd, [Rn, #imm5<<2]
                    offset_imm = ((h >> 6) & 0x1F) << 2
                    accessed_registers.add(offset_imm)
                elif (h & 0xF800) == 0x6800: # LDR Rd, [Rn, #imm5<<2]
                    offset_imm = ((h >> 6) & 0x1F) << 2
                    accessed_registers.add(offset_imm)

        reg_desc = {
            0x00: "CTL0 (Pin 0-7 Konfigürasyonu)",
            0x04: "CTL1 (Pin 8-15 Konfigürasyonu)",
            0x08: "ISTAT (Giriş Veri Oku - Tuş/Sensör)",
            0x0C: "OCTL (Çıkış Veri Yaz)",
            0x10: "BOP (Bit Set/Clear)",
            0x14: "BC (Bit Clear)"
        }
        for r_off in sorted(accessed_registers):
            desc = reg_desc.get(r_off, f"Offset +0x{r_off:02X}")
            print(f"    * Erişim: +0x{r_off:02X} -> {desc}")

    # 3. Giriş Okuma (ISTAT) Taraması: Tuşlar, PTT ve Döner Enkoder
    print("\n[+] Donanım Giriş Hatları (ISTAT - Tuş Takımı / PTT / Enkoder Taraması):")
    # Kodda ISTAT (+0x08) okunduktan sonra ANDS / TST yapılan bitmask'leri arayalım
    for name, p_addr in GPIO_PORTS.items():
        pattern = struct.pack("<I", p_addr)
        for off in port_refs[name]:
            # Yakın bölgede ISTAT okuma (+0x08) var mı?
            chunk = data[max(0, off-32):min(len(data), off+64)]
            # 0x68?? where offset is 0x08 (imm5 = 2)
            for idx in range(0, len(chunk)-4, 2):
                ins = struct.unpack("<H", chunk[idx:idx+2])[0]
                if (ins & 0xFFC0) == 0x6880: # LDR Rx, [Ry, #8] -> ISTAT okuma!
                    code_addr = BASE_ADDR + max(0, off-32) + idx
                    print(f"    * {name} ISTAT Okuma Tespit Edildi: Kod Adresi 0x{code_addr:08X}")

if __name__ == "__main__":
    cdd = r"C:\Users\Fatih ATCI\Desktop\Anytone\AT-D878-Open-Firmware\tools\firmware_analyzer\D878UVII_V4.00 FW\D878UVII_V4.00_20250725.CDD"
    analyze_gpio(cdd)
