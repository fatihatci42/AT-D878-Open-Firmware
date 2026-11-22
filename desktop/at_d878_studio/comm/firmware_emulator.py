#!/usr/bin/env python3
"""
AT-D878 Studio - Raw image vector checks and partial ARM CPU sandbox.
Memory-mapped radio peripherals and display behavior are not modeled.
"""

import os
import re
import struct
import unicorn
from unicorn import arm_const

FLASH_BASE = 0x08000000
FLASH_APP_BASE = 0x08004000
FLASH_SIZE = 1024 * 1024  # 1 MB Flash
FLASH_APP_SIZE = FLASH_SIZE - (FLASH_APP_BASE - FLASH_BASE)
RAM_BASE = 0x20000000
RAM_SIZE = 96 * 1024      # 96 KB SRAM

class FirmwareReport:
    def __init__(self):
        self.filepath = ""
        self.filesize = 0
        self.initial_sp = 0
        self.reset_handler = 0
        self.vectors_valid = False
        self.vector_check_msg = ""
        self.detected_model = "Bulunamadı"
        self.detected_callsign = "Bulunamadı"
        self.vfo_a_freq = "Bulunamadı"
        self.vfo_b_freq = "Bulunamadı"
        self.vfo_a_mode = "FM"
        self.vfo_b_mode = "DMR"
        self.instructions_executed = 0
        self.emulation_status = "Çalıştırılmadı"
        self.emulation_error = ""
        self.strings_found = []
        self.pc_trace = []
        self.registers = {}

class FirmwareEmulator:
    def __init__(self):
        self.report = FirmwareReport()

    def analyze_and_emulate(self, filepath, max_cycles=5000):
        self.report = FirmwareReport()
        self.report.filepath = filepath

        if not os.path.isfile(filepath):
            self.report.vector_check_msg = "Dosya bulunamadı."
            return self.report

        try:
            with open(filepath, "rb") as f:
                data = f.read(FLASH_APP_SIZE + 1)
        except OSError as exc:
            self.report.vector_check_msg = f"Dosya okunamadı: {exc}"
            return self.report

        self.report.filesize = len(data)
        if len(data) < 8:
            self.report.vector_check_msg = "Dosya vektör tablosunu okuyacak kadar büyük değil."
            return self.report
        if len(data) > FLASH_APP_SIZE:
            self.report.vector_check_msg = f"İmaj uygulama alanını aşıyor ({len(data)} byte > {FLASH_APP_SIZE} byte)."
            return self.report

        # 1. Vektör Tablosu İncelemesi
        sp, reset = struct.unpack("<II", data[:8])
        self.report.initial_sp = sp
        self.report.reset_handler = reset

        # GD32F303 SRAM: 0x20000000 - 0x20017FFF (96KB)
        valid_sp = (RAM_BASE <= sp <= RAM_BASE + RAM_SIZE) and (sp % 8 == 0)
        reset_address = reset & ~1
        # Firmware image must begin at the application base and use Thumb state.
        valid_reset = (
            bool(reset & 1)
            and FLASH_APP_BASE <= reset_address < FLASH_APP_BASE + len(data)
            and reset_address < FLASH_BASE + FLASH_SIZE
        )

        self.report.vectors_valid = valid_sp and valid_reset

        # These checks only assess a raw application image's vector table. They
        # do not prove firmware safety, integrity, or hardware compatibility.
        if not valid_sp:
            self.report.vector_check_msg = f"Vektör kontrolü başarısız: SRAM sınırı / 8-byte hizası geçersiz (SP=0x{sp:08X})."
        elif not (reset & 1):
            self.report.vector_check_msg = f"Vektör kontrolü başarısız: Reset Handler Thumb adresi değil (0x{reset:08X})."
        elif not valid_reset:
            self.report.vector_check_msg = f"Vektör kontrolü başarısız: Reset Handler imajın uygulama aralığında değil (0x{reset:08X})."
        else:
            self.report.vector_check_msg = (
                "Vektör tablosu temel adres kontrollerini geçti. Bu, firmware'in güvenli, "
                "sağlam veya cihazla uyumlu olduğunu kanıtlamaz."
            )

        # 3. Gömülü Veri & Arayüz Metinlerini Tarama
        ascii_matches = re.findall(b"[A-Za-z0-9_\\-\\?\\!\\:\\%\\.\\[\\]]{4,32}", data)
        str_list = [m.decode("latin1") for m in ascii_matches]
        self.report.strings_found = str_list[:40]

        for s in str_list:
            if "D878" in s.upper():
                self.report.detected_model = s
            if any(call in s for call in ["TA5THK", "TA1", "TA2", "TA3", "TA4", "TA5"]):
                self.report.detected_callsign = s
            if re.match(r"14[4-6]\.\d{3,5}", s):
                self.report.vfo_a_freq = s
            if re.match(r"43[0-9]\.\d{3,5}", s):
                self.report.vfo_b_freq = s

        # 4. Unicorn Engine ile ARM Cortex-M4 Donanım Emülasyonu
        if self.report.vectors_valid:
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
                instruction_count = [0]
                def trace_hook(uc, addr, size, user_data):
                    instruction_count[0] += 1
                    if len(step_trace) < 25:
                        step_trace.append(f"0x{addr:08X} (Boyut: {size}B)")

                mu.hook_add(unicorn.UC_HOOK_CODE, trace_hook)

                # This is a partial CPU sandbox: memory-mapped peripherals are
                # zero-filled and do not model the radio's actual hardware.
                max_cycles = max(1, min(int(max_cycles), 1_000_000))
                try:
                    mu.emu_start(reset, FLASH_APP_BASE + len(data), count=max_cycles)
                    self.report.emulation_status = "Sınırda durdu"
                except unicorn.UcError as exc:
                    self.report.emulation_status = "Emülasyon hatası"
                    self.report.emulation_error = str(exc)

                self.report.instructions_executed = instruction_count[0]
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
            except (unicorn.UcError, ValueError, TypeError) as emu_err:
                self.report.emulation_status = "Başlatılamadı"
                self.report.emulation_error = str(emu_err)

        return self.report
