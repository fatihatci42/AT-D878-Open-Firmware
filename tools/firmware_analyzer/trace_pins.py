#!/usr/bin/env python3
"""
GD32 SPL Function Call & Pin Parameter Extractor
Orijinal V4.00 firmware binary'sinde gpio_init, gpio_bit_set, gpio_bit_reset ve
gpio_input_bit_get çağrılarını bularak R0 (Port) ve R1 (Pin Maskesi) parametrelerini çıkarır.
"""

import sys
import struct
import os

BASE_ADDR = 0x08004000

GPIO_PORTS = {
    0x40010800: "GPIOA",
    0x40010C00: "GPIOB",
    0x40011000: "GPIOC",
    0x40011400: "GPIOD",
    0x40011800: "GPIOE",
}

def decode_pin_mask(mask):
    pins = []
    for i in range(16):
        if (mask >> i) & 1:
            pins.append(f"Pin_{i}")
    return "|".join(pins) if pins else f"0x{mask:04X}"

def trace_gpio_calls(cdd_path):
    with open(cdd_path, "rb") as f:
        data = f.read()

    print("=" * 70)
    print("ANYTONE AT-D878UVII GPIO ÇAĞRI VE PİN PARAMETRE TARAYICISI")
    print("=" * 70)

    # 1. Literal Pool'da port adreslerinin konumlarını bul
    port_locs = {}
    for p_addr, p_name in GPIO_PORTS.items():
        pattern = struct.pack("<I", p_addr)
        offsets = []
        pos = 0
        while True:
            idx = data.find(pattern, pos)
            if idx == -1: break
            offsets.append(idx)
            pos = idx + 4
        port_locs[p_addr] = offsets

    # 2. Literal Pool'u yükleyen LDR r0/r1, [pc, #imm] komutlarını bul
    # LDR Rt, [pc, #imm] -> 0x4800 | (Rt << 8) | imm8 (offset = imm8 * 4)
    ldr_instructions = []
    for i in range(0, len(data) - 4, 2):
        half = struct.unpack("<H", data[i:i+2])[0]
        if (half & 0xF800) == 0x4800: # LDR Rt, [PC, #imm8]
            rt = (half >> 8) & 0x07
            imm8 = half & 0xFF
            target_offset = ((i + 4) & ~3) + (imm8 * 4)
            if target_offset < len(data) - 4:
                val = struct.unpack("<I", data[target_offset:target_offset+4])[0]
                if val in GPIO_PORTS:
                    ldr_instructions.append((i, rt, val, GPIO_PORTS[val]))

    print(f"[+] Toplam {len(ldr_instructions)} adet GPIO Port yükleme (LDR) komutu bulundu.")

    # 3. Bu LDR'lerin hemen sonrasındaki komutları analiz et
    # Genellikle:
    # LDR R0, =GPIOA
    # MOVS R1, #Pin (veya LDR R1, =PinMask)
    # BL gpio_bit_set / BL gpio_bit_reset / BL gpio_input_bit_get
    detected_pins = {name: set() for name in GPIO_PORTS.values()}

    for code_off, rt, port_val, port_name in ldr_instructions:
        code_addr = BASE_ADDR + code_off
        # İlerleyen 16 byte'ı tara
        window = data[code_off:min(len(data), code_off + 24)]
        # MOVS R1, #imm8 -> 0x2100 | imm8
        for w_idx in range(2, len(window) - 2, 2):
            w_half = struct.unpack("<H", window[w_idx:w_idx+2])[0]
            if (w_half & 0xFF00) == 0x2100: # MOVS R1, #imm8
                imm8 = w_half & 0xFF
                pin_str = decode_pin_mask(imm8)
                detected_pins[port_name].add(pin_str)
            elif (w_half & 0xF800) == 0x4900: # LDR R1, [PC, #imm8]
                r1_imm = w_half & 0xFF
                r1_target = ((code_off + w_idx + 4) & ~3) + (r1_imm * 4)
                if r1_target < len(data) - 4:
                    r1_val = struct.unpack("<I", data[r1_target:r1_target+4])[0]
                    if 0 < r1_val <= 0xFFFF:
                        pin_str = decode_pin_mask(r1_val)
                        detected_pins[port_name].add(pin_str)

    print("\n[+] Tespit Edilen Aktif Port ve Pin Atamaları:")
    for port_name, pins in detected_pins.items():
        if pins:
            print(f"  * {port_name}:")
            for p in sorted(pins):
                print(f"      - {p}")
        else:
            print(f"  * {port_name}: (Doğrudan SPL register adresleriyle yönetiliyor)")

if __name__ == "__main__":
    cdd = r"C:\Users\Fatih ATCI\Desktop\Anytone\AT-D878-Open-Firmware\tools\firmware_analyzer\D878UVII_V4.00 FW\D878UVII_V4.00_20250725.CDD"
    trace_gpio_calls(cdd)
