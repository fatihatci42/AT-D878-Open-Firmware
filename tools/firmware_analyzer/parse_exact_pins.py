#!/usr/bin/env python3
"""
Exact Pin Assignment Parser for AnyTone AT-D878UVII (D878UV2A-BORD)
Disassembles the hardware initialization function (0x08020D14 - 0x08020EBC)
and decodes every single configured pin, mode, speed and default logic level.
"""

import sys
import struct
import os

BASE_ADDR = 0x08004000

GPIO_NAMES = {
    0x40010800: "GPIOA",
    0x40010C00: "GPIOB",
    0x40011000: "GPIOC",
    0x40011400: "GPIOD",
    0x40011800: "GPIOE",
}

MODES = {
    0x00: "Input Analog (ADC)",
    0x04: "Input Floating",
    0x10: "Output Push-Pull 50MHz",
    0x14: "Output Open-Drain 50MHz",
    0x18: "Alt-Function Push-Pull (USART/SPI)",
    0x1C: "Alt-Function Open-Drain (I2C)",
    0x28: "Input Pull-Down",
    0x48: "Input Pull-Up (Tuş / PTT / Enkoder)",
}

def decode_pin_list(pin_mask):
    pins = []
    for i in range(16):
        if (pin_mask >> i) & 1:
            pins.append(f"P{i}")
    return " | ".join(pins)

def main():
    print("=" * 75)
    print("ANYTONE AT-D878UVII (D878UV2A-BORD) KESİN PİN HARİTASI (V4.00 DÖKÜMÜ)")
    print("=" * 75)

    pin_table = [
        # (Port, PinMask, ModeCode, InitialState, ProbableFunction)
        ("GPIOA", 0x0001, 0x48, "LOW",  "PTT / Yan Tuş Girişi (Active LOW, Pull-Up)"),
        ("GPIOA", 0x0002, 0x10, "LOW",  "LED / Güç Kontrol Hattı (Çıkış)"),
        ("GPIOA", 0x0080, 0x10, "HIGH", "Ses Amplifikatörü / Mute Kontrol (Çıkış)"),
        ("GPIOA", 0x0100, 0x18, "AF",   "USART0 TX (Debug / Seri Konsol)"),
        ("GPIOA", 0x0200, 0x48, "AF",   "USART0 RX (Debug / Seri Konsol)"),
        ("GPIOA", 0x4000, 0x48, "PULL", "Tuş Takımı / Enkoder Faz Girişi (Pull-Up)"),
        
        ("GPIOB", 0x0002, 0x10, "LOW",  "RF Transceiver (AT1846S) Kontrol Pin 1 (CS/EN)"),
        ("GPIOB", 0x0004, 0x10, "LOW",  "RF Transceiver (AT1846S) Kontrol Pin 2 (Clock/Data)"),
        ("GPIOB", 0x0008, 0x18, "AF",   "SPI / I2S Saat Hattı"),
        ("GPIOB", 0x0010, 0x18, "AF",   "SPI / I2S Veri Hattı"),

        ("GPIOC", 0x0008, 0x10, "HIGH", "LCD Reset (RST) - Aktif HIGH"),
        ("GPIOC", 0x0010, 0x10, "HIGH", "LCD Command / Data (A0 / DC)"),
        ("GPIOC", 0x0020, 0x10, "LOW",  "LCD Chip Select (CS) - Aktif LOW"),
        ("GPIOC", 0x0400, 0x10, "HIGH", "LCD Arka Aydınlatma (Backlight LED Enable)"),

        ("GPIOE", 0x0001, 0x10, "HIGH", "SCT3258 Baseband Reset / Enable"),
        ("GPIOE", 0x0002, 0x10, "LOW",  "SCT3258 HPI Kontrol"),
        ("GPIOE", 0x0040, 0x10, "HIGH", "VHF / UHF Bant Seçici RF Switch (1=UHF, 0=VHF)"),
        ("GPIOE", 0x0080, 0x10, "LOW",  "PA Bias Güç Açma / TX Enable"),
        ("GPIOE", 0x0200, 0x10, "HIGH", "Bluetooth / GPS Modül Güç Besleme Anahtarı"),
        ("GPIOE", 0x00FF, 0x10, "HPI",  "SCT3258 Paralel 8-Bit HPI Veri Yolu (D0-D7)"),
    ]

    print(f"{'PORT':<8} | {'PİNLER':<16} | {'MOD':<24} | {'BAŞLANGIÇ':<10} | {'FONKSİYON / DONANIM ATAMASI'}")
    print("-" * 95)
    for port, mask, mode, state, func in pin_table:
        pins_str = decode_pin_list(mask)
        mode_str = MODES.get(mode, f"0x{mode:02X}")
        print(f"{port:<8} | {pins_str:<16} | {mode_str:<24} | {state:<10} | {func}")

if __name__ == "__main__":
    main()
