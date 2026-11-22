#!/usr/bin/env python3
"""
ARM Cortex-M4 Thumb-2 Entry & Branch Tracer
Reset Handler (0x08004C90) ve SystemInit fonksiyonlarını takip ederek
donanım başlatma ve GPIO konfigürasyon fonksiyonlarının adreslerini bulur.
"""

import struct
import sys

BASE_ADDR = 0x08004000

def decode_thumb(data, start_off, count=50):
    idx = start_off
    instructions = []
    end = min(len(data), start_off + count * 4)

    while idx < end:
        addr = BASE_ADDR + idx
        h1 = struct.unpack("<H", data[idx:idx+2])[0]

        # 32-bit Thumb-2 mi kontrol et (0xE800 - 0xFFFF aralığı)
        is_32bit = (h1 & 0xF800) in (0xE800, 0xF000, 0xF800)

        if is_32bit and idx + 4 <= len(data):
            h2 = struct.unpack("<H", data[idx+2:idx+4])[0]
            # BL / BLX (32-bit branch with link)
            # 11110 S imm10 | 11 J1 1 J2 imm11
            if (h1 & 0xF800) == 0xF000 and (h2 & 0xD000) == 0xD000:
                s = (h1 >> 10) & 1
                imm10 = h1 & 0x3FF
                j1 = (h2 >> 13) & 1
                j2 = (h2 >> 11) & 1
                imm11 = h2 & 0x7FF
                i1 = not (j1 ^ s)
                i2 = not (j2 ^ s)
                imm32 = (s << 24) | (i1 << 23) | (i2 << 22) | (imm10 << 12) | (imm11 << 1)
                if s:
                    imm32 -= (1 << 25)
                target = addr + 4 + imm32
                instructions.append((addr, f"BL 0x{target:08X} (Ofset: 0x{target - BASE_ADDR:06X})"))
            else:
                instructions.append((addr, f"32-bit: 0x{h1:04X} 0x{h2:04X}"))
            idx += 4
        else:
            # 16-bit Thumb
            # LDR Rt, [PC, #imm8]
            if (h1 & 0xF800) == 0x4800:
                rt = (h1 >> 8) & 7
                imm8 = h1 & 0xFF
                lit_off = ((idx + 4) & ~3) + (imm8 * 4)
                if lit_off + 4 <= len(data):
                    val = struct.unpack("<I", data[lit_off:lit_off+4])[0]
                    instructions.append((addr, f"LDR R{rt}, [PC, #{imm8*4}] -> =0x{val:08X}"))
                else:
                    instructions.append((addr, f"LDR R{rt}, [PC, #{imm8*4}]"))
            # B <imm>
            elif (h1 & 0xF800) == 0xE000:
                imm11 = h1 & 0x7FF
                if imm11 & 0x400: imm11 -= 0x800
                target = addr + 4 + (imm11 * 2)
                instructions.append((addr, f"B 0x{target:08X}"))
            # BX Rm
            elif (h1 & 0xFF87) == 0x4700:
                rm = (h1 >> 3) & 0xF
                instructions.append((addr, f"BX R{rm}"))
            # BLX Rm
            elif (h1 & 0xFF87) == 0x4780:
                rm = (h1 >> 3) & 0xF
                instructions.append((addr, f"BLX R{rm}"))
            else:
                instructions.append((addr, f"16-bit: 0x{h1:04X}"))
            idx += 2

    return instructions

def main():
    cdd = r"C:\Users\Fatih ATCI\Desktop\Anytone\AT-D878-Open-Firmware\tools\firmware_analyzer\D878UVII_V4.00 FW\D878UVII_V4.00_20250725.CDD"
    with open(cdd, "rb") as f:
        data = f.read()

    reset_handler_addr = 0x08004C90
    reset_off = reset_handler_addr - BASE_ADDR

    print("=" * 60)
    print(f"RESET HANDLER TRACE (Adres: 0x{reset_handler_addr:08X}, Ofset: 0x{reset_off:06X})")
    print("=" * 60)

    instrs = decode_thumb(data, reset_off, 40)
    for addr, s in instrs:
        print(f"  0x{addr:08X}: {s}")

if __name__ == "__main__":
    main()
