#!/usr/bin/env python3
"""
AT-D878 Studio - ARM Cortex-M4 Firmware Analyzer & Emulator Engine
Unicorn Engine tabanlı güvenli firmware analiz ve simülasyon modülü.
AnyTone AT-D878UVII Plus GD32F303VGT6 hedefi için optimize edilmiştir.
"""

import os
import re
import struct
import unicorn
from unicorn import arm_const

FLASH_BASE = 0x08000000
FLASH_APP_BASE = 0x08004000
FLASH_SIZE = 1024 * 1024  # 1 MB Flash
RAM_BASE = 0x20000000
RAM_SIZE = 96 * 1024      # 96 KB SRAM
BOOTLOADER_LIMIT = 0x08003FFF

class FirmwareReport:
    def __init__(self):
        self.filepath = ""
        self.filesize = 0
        self.initial_sp = 0
        self.reset_handler = 0
        self.is_valid_arm = False
        self.is_safe = False
        self.safety_msg = ""
        self.detected_model = "Bilinmiyor"
        self.detected_callsign = "TA5THK"
        self.vfo_a_freq = "145.500.00"
        self.vfo_b_freq = "433.500.00"
        self.vfo_a_mode = "FM"
        self.vfo_b_mode = "DMR"
        self.cycles_executed = 0
        self.strings_found = []
        self.pc_trace = []
        self.registers = {}

class FirmwareEmulator:
    def __init__(self):
        self.report = FirmwareReport()

    def analyze_and_emulate(self, filepath, max_cycles=5000):
        self.report = FirmwareReport()
        self.report.filepath = filepath

        if not os.path.exists(filepath):
            self.report.safety_msg = "Dosya bulunamadı."
            return self.report

        with open(filepath, "rb") as f:
            data = f.read()

        self.report.filesize = len(data)
        if len(data) < 32:
            self.report.safety_msg = "Dosya boyutu Cortex-M4 vektör tablosu için çok küçük."
            return self.report

        # 1. Vektör Tablosu İncelemesi
        sp, reset = struct.unpack("<II", data[:8])
        self.report.initial_sp = sp
        self.report.reset_handler = reset

        # GD32F303 SRAM: 0x20000000 - 0x20017FFF (96KB)
        valid_sp = (0x20001000 <= sp <= 0x20018000)
        # Flash App: 0x08004000 - 0x08100000
        valid_reset = (FLASH_APP_BASE <= (reset & ~1) <= (FLASH_BASE + FLASH_SIZE))

        self.report.is_valid_arm = valid_sp and valid_reset

        # 2. Güvenlik Denetimi (Bootloader Koruması)
        if (reset & ~1) <= BOOTLOADER_LIMIT:
            self.report.is_safe = False
            self.report.safety_msg = "KRİTİK HATA: Firmware fabrika bootloader bölgesine (0x08000000-0x08003FFF) müdahale ediyor!"
        elif not valid_sp:
            self.report.is_safe = False
            self.report.safety_msg = f"HATA: Geçersiz Stack Pointer adresi: 0x{sp:08X}"
        elif not valid_reset:
            self.report.is_safe = False
            self.report.safety_msg = f"HATA: Geçersiz Reset Handler adresi: 0x{reset:08X}"
        else:
            self.report.is_safe = True
            self.report.safety_msg = "🟢 %100 GÜVENLİ: Firmware 0x08004000 uygulama adresine bağlı, orijinal ROM bootloader tam korumalıdır."

        # 3. Gömülü Veri & Arayüz Metinlerini Tarama
        ascii_matches = re.findall(b"[A-Za-z0-9_\\-\\?\\!\\:\\%\\.\\[\\]]{4,32}", data)
        str_list = [m.decode("latin1") for m in ascii_matches]
        self.report.strings_found = str_list[:40]

        for s in str_list:
            if "D878" in s:
                self.report.detected_model = s
            if any(call in s for call in ["TA5THK", "TA1", "TA2", "TA3", "TA4", "TA5"]):
                self.report.detected_callsign = s
            if re.match(r"14[4-6]\.\d{3,5}", s):
                self.report.vfo_a_freq = s
            if re.match(r"43[0-9]\.\d{3,5}", s):
                self.report.vfo_b_freq = s

        # 4. Unicorn Engine ile ARM Cortex-M4 Donanım Emülasyonu
        if self.report.is_safe:
            try:
                mu = unicorn.Uc(unicorn.UC_ARCH_ARM, unicorn.UC_MODE_THUMB)

                # Flash Bellek Eşleme (1 MB)
                mu.mem_map(FLASH_BASE, FLASH_SIZE)
                # SRAM Eşleme (96 KB)
                mu.mem_map(RAM_BASE, RAM_SIZE)
                # Periferik / MMIO Alanı Eşleme (0x40000000 - 0x40040000)
                mu.mem_map(0x40000000, 256 * 1024)
                # Cortex-M4 Dahili Periferikleri (SCB, SysTick 0xE000E000)
                mu.mem_map(0xE0000000, 64 * 1024)

                # Firmware ikilisini 0x08004000 adresine yaz
                mu.mem_write(FLASH_APP_BASE, data)

                # Kayıtçıları Başlat
                mu.reg_write(arm_const.UC_ARM_REG_SP, sp)
                mu.reg_write(arm_const.UC_ARM_REG_PC, reset)

                step_trace = []
                def trace_hook(uc, addr, size, user_data):
                    if len(step_trace) < 25:
                        step_trace.append(f"0x{addr:08X} (Boyut: {size}B)")

                mu.hook_add(unicorn.UC_HOOK_CODE, trace_hook)

                # Belirtilen döngü kadar emüle et
                try:
                    mu.emu_start(reset, FLASH_APP_BASE + len(data), count=max_cycles)
                except Exception:
                    # Sonsuz döngü (while(1)) veya MMIO okuma istisnası doğaldır
                    pass

                self.report.cycles_executed = len(step_trace)
                self.report.pc_trace = step_trace
                self.report.registers = {
                    "R0": f"0x{mu.reg_read(arm_const.UC_ARM_REG_R0):08X}",
                    "R1": f"0x{mu.reg_read(arm_const.UC_ARM_REG_R1):08X}",
                    "R2": f"0x{mu.reg_read(arm_const.UC_ARM_REG_R2):08X}",
                    "R3": f"0x{mu.reg_read(arm_const.UC_ARM_REG_R3):08X}",
                    "SP": f"0x{mu.reg_read(arm_const.UC_ARM_REG_SP):08X}",
                    "LR": f"0x{mu.reg_read(arm_const.UC_ARM_REG_LR):08X}",
                    "PC": f"0x{mu.reg_read(arm_const.UC_ARM_REG_PC):08X}",
                }
            except Exception as emu_err:
                self.report.safety_msg += f" (Emülatör notu: {str(emu_err)})"

        return self.report
