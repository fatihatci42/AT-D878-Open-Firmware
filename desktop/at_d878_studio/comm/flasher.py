#!/usr/bin/env python3
"""Serial discovery and non-destructive AT-D878 package checks.

The repository does not contain a verified write protocol for any target. This
module deliberately keeps real writes disabled until target-specific framing
and recovery behavior have been validated against hardware.
"""

import os
import struct
import time

import serial

CMD_ACK = b"\x06"
CMD_PROGRAM = b"PROGRAM"
CMD_QUERY_ID = b"\x02"

FLASH_APP_BASE = 0x08004000
FLASH_APP_SIZE = (1024 * 1024) - (FLASH_APP_BASE - 0x08000000)
RAM_BASE = 0x20000000
RAM_SIZE = 96 * 1024
BLOCK_SIZE = 1024
MAX_PACKAGE_SIZE = 16 * 1024 * 1024

FLASH_TARGETS = {
    "mcu": "Ana MCU (GD32)",
    "icons": "İkon / font flash",
    "sct3258": "DMR baseband (SCT3258)",
    "linkboard": "LinkBoard (APRS / Bluetooth)",
}


class AnyToneFlasher:
    def __init__(self, port, baudrate=921600, timeout=1.5):
        self.port = port
        self.baudrate = baudrate
        self.timeout = timeout
        self.ser = None
        self.model_info = ""
        self.hw_version = ""

    def connect(self):
        try:
            self.ser = serial.Serial(
                port=self.port,
                baudrate=self.baudrate,
                bytesize=serial.EIGHTBITS,
                parity=serial.PARITY_NONE,
                stopbits=serial.STOPBITS_ONE,
                timeout=self.timeout,
                write_timeout=self.timeout,
            )
            time.sleep(0.05)
            return True, f"Port {self.port} açıldı ({self.baudrate} baud)."
        except serial.SerialException as exc:
            return False, f"Seri port açılamadı: {exc}"
        except Exception as exc:
            return False, f"Bağlantı hatası: {exc}"

    def disconnect(self):
        if self.ser and self.ser.is_open:
            try:
                self.ser.close()
            except serial.SerialException:
                pass

    def ping_device(self):
        """Query and verify the D878 model; an arbitrary QX reply is insufficient."""
        if not self.ser or not self.ser.is_open:
            ok, message = self.connect()
            if not ok:
                return False, message

        try:
            self.ser.reset_input_buffer()
            self.ser.reset_output_buffer()
            self.ser.write(CMD_PROGRAM)
            self.ser.flush()
            response = self.ser.read(16)
            if b"QX" not in response:
                detail = response.hex() if response else "zaman aşımı"
                return False, f"PROGRAM yanıtı tanınmadı (0x{detail})."

            self.ser.write(CMD_QUERY_ID)
            self.ser.flush()
            response = self.ser.read(32)
            clean = response.replace(CMD_ACK, b"").replace(CMD_QUERY_ID, b"")
            fields = [part.decode("ascii", errors="ignore") for part in clean.split(b"\x00") if part]
            model = fields[0] if fields else ""
            if not any(token in model.upper() for token in ("D878", "878UV")):
                detail = response.hex() if response else "zaman aşımı"
                return False, f"D878 model kimliği doğrulanamadı (0x{detail})."

            self.model_info = model
            self.hw_version = fields[1] if len(fields) > 1 else "Bilinmiyor"
            self.ser.write(CMD_ACK)
            self.ser.flush()
            return True, f"D878 modeli doğrulandı: {self.model_info} / {self.hw_version}"
        except Exception as exc:
            return False, f"İletişim sırasında hata: {exc}"

    def flash_binary(self, bin_path, progress_callback=None, dry_run=True, target="mcu"):
        """Check a package and simulate progress without sending data to the radio."""
        if target not in FLASH_TARGETS:
            return False, f"Bilinmeyen yazma hedefi: {target}"
        if not os.path.isfile(bin_path):
            return False, "Seçilen dosya bulunamadı."

        try:
            total_bytes = os.path.getsize(bin_path)
            if total_bytes == 0 or total_bytes > MAX_PACKAGE_SIZE:
                return False, f"Dosya boyutu desteklenmiyor: {total_bytes} byte."
            with open(bin_path, "rb") as package:
                data = package.read()
        except OSError as exc:
            return False, f"Dosya okunamadı: {exc}"

        if target == "mcu":
            if total_bytes < 8 or total_bytes > FLASH_APP_SIZE:
                return False, f"MCU imajı uygulama alanı sınırında değil ({total_bytes} byte)."
            sp, reset = struct.unpack("<II", data[:8])
            reset_address = reset & ~1
            valid_sp = RAM_BASE <= sp <= RAM_BASE + RAM_SIZE and sp % 8 == 0
            valid_reset = (reset & 1) == 1 and FLASH_APP_BASE <= reset_address < FLASH_APP_BASE + total_bytes
            if not valid_sp or not valid_reset:
                return False, f"Geçersiz MCU vektör tablosu (SP=0x{sp:08X}, Reset=0x{reset:08X})."

        if not dry_run:
            return False, (
                "Gerçek yazma devre dışı: bu projede hiçbir hedef için doğrulanmış "
                "bootloader yazma protokolü yok. Cihaza veri gönderilmedi."
            )

        total_blocks = max(1, (total_bytes + BLOCK_SIZE - 1) // BLOCK_SIZE)
        last_percent = -1
        for block_index in range(total_blocks):
            percent = int(((block_index + 1) / total_blocks) * 100)
            if progress_callback and percent != last_percent:
                progress_callback(percent, f"Ön kontrol: %{percent} — {FLASH_TARGETS[target]}")
                last_percent = percent

        if target == "mcu":
            detail = "vektör tablosu ve uygulama alanı boyutu doğrulandı"
        else:
            detail = "dosya okunabildi ve boyut sınırında; hedef paket biçimi doğrulanmadı"
        return True, (
            f"Dry-run tamamlandı: {FLASH_TARGETS[target]}, {total_bytes} byte; "
            f"{detail}. Cihaza yazılmadı."
        )
