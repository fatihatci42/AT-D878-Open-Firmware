#!/usr/bin/env python3
"""
AnyTone AT-D878UVII Full GPIO Pin Configuration Extractor
Tüm firmware binary'sindeki GPIO_Init (0x080057B8) çağrılarını bulur,
yapılandırılan Port, Pin, Mod (Giriş/Çıkış) ve Hız parametrelerini döker.
"""

import sys
import struct
import os

BASE_ADDR = 0x08004000
GPIO_INIT_ADDR = 0x080057B8

GPIO_PORTS = {
    0x40010800: "GPIOA",
    0x40010C00: "GPIOB",
    0x40011000: "GPIOC",
    0x40011400: "GPIOD",
    0x40011800: "GPIOE",
}

MODE_STRINGS = {
    0x00: "IN_ANALOG (0x00)",
    0x04: "IN_FLOATING (0x04)",
    0x28: "IN_PULLDOWN (0x28)",
    0x48: "IN_PULLUP (0x48)",
    0x10: "OUT_PP (Push-Pull)",
    0x14: "OUT_OD (Open-Drain)",
    0x18: "AF_PP (Alt-Func Push-Pull - SPI/UART)",
    0x1C: "AF_OD (Alt-Func Open-Drain - I2C)",
}

def decode_pins(pin_mask):
    pins = []
    for i in range(16):
        if (pin_mask >> i) & 1:
            pins.append(f"P{i}")
    return ",".join(pins) if pins else f"0x{pin_mask:04X}"

def main():
    cdd = r"C:\Users\Fatih ATCI\Desktop\Anytone\AT-D878-Open-Firmware\tools\firmware_analyzer\D878UVII_V4.00 FW\D878UVII_V4.00_20250725.CDD"
    with open(cdd, "rb") as f:
        data = f.read()

    print("=" * 70)
    print("ANYTONE AT-D878UVII PLUS (V4.00) TÜM GPIO_INIT ÇAĞRI ANALİZİ")
    print(f"Hedef Fonksiyon: GPIO_Init (0x{GPIO_INIT_ADDR:08X})")
    print("=" * 70)

    # BL 0x080057B8 çağrılarını ara
    # BL komutunun opcode hesaplaması:
    # PC = call_addr + 4
    # offset = 0x080057B8 - PC
    call_sites = []
    for idx in range(0, len(data) - 4, 2):
        addr = BASE_ADDR + idx
        h1, h2 = struct.unpack("<HH", data[idx:idx+4])
        if (h1 & 0xF800) == 0xF000 and (h2 & 0xD000) == 0xD000:
            s = (h1 >> 10) & 1
            imm10 = h1 & 0x3FF
            j1 = (h2 >> 13) & 1
            j2 = (h2 >> 11) & 1
            imm11 = h2 & 0x7FF
            i1 = not (j1 ^ s)
            i2 = not (j2 ^ s)
            imm32 = (s << 24) | (i1 << 23) | (i2 << 22) | (imm10 << 12) | (imm11 << 1)
            if s: imm32 -= (1 << 25)
            target = addr + 4 + imm32
            if target == GPIO_INIT_ADDR:
                call_sites.append(idx)

    print(f"[+] Toplam {len(call_sites)} adet GPIO_Init çağrısı tespit edildi!\n")

    inits = []
    for call_off in call_sites:
        call_addr = BASE_ADDR + call_off
        # Çağrıdan önceki 32 byte'ı incele: Port R0'a, InitStruct R1'e yüklenir
        window = data[max(0, call_off - 40):call_off]
        
        # Port LDR'ını ara
        port_found = None
        for i in range(len(window) - 4, 0, -2):
            h = struct.unpack("<H", window[i:i+2])[0]
            if (h & 0xF800) == 0x4800: # LDR R0, [PC, #imm]
                rt = (h >> 8) & 7
                if rt == 0:
                    imm = h & 0xFF
                    actual_off = max(0, call_off - 40) + i
                    lit_target = ((actual_off + 4) & ~3) + (imm * 4)
                    if lit_target + 4 <= len(data):
                        val = struct.unpack("<I", data[lit_target:lit_target+4])[0]
                        if val in GPIO_PORTS:
                            port_found = GPIO_PORTS[val]
                            break

        inits.append((call_addr, port_found))

    # Portlara göre grupla
    by_port = {}
    for addr, p in inits:
        p_name = p if p else "Bilinmeyen/Dinamik"
        by_port.setdefault(p_name, []).append(f"0x{addr:08X}")

    for p_name, addrs in sorted(by_port.items()):
        print(f"  * {p_name}: {len(addrs)} çağrı ({', '.join(addrs[:5])}...)")

if __name__ == "__main__":
    main()
