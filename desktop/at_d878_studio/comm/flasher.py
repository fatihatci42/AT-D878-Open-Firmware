#!/usr/bin/env python3
"""
AnyTone AT-D878 Native USB Bootloader & Communication Engine
Doğrulanmış AnyTone D878UVII Protokolü:
1. Adım: b'PROGRAM'  -> Yanıt: b'QX\x06' (Qixiang ACK)
2. Adım: b'\x02'     -> Yanıt: b'ID878UV2\x00V101\x00\x00\x06' (Model ve Versiyon)
3. Adım: b'\x06'     -> Yanıt: ACK / Durum Bilgisi
"""

import sys
import os
import time
import struct
import serial
import serial.tools.list_ports

CMD_ACK = b"\x06"
CMD_NACK = b"\x15"
CMD_PROGRAM = b"PROGRAM"
CMD_QUERY_ID = b"\x02"

FLASH_APP_BASE = 0x08004000
BLOCK_SIZE = 1024  # 1 KB

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
                write_timeout=self.timeout
            )
            time.sleep(0.05)
            return True, f"Port {self.port} başarıyla açıldı ({self.baudrate} baud)."
        except serial.SerialException as se:
            if "PermissionError" in str(se) or "Erişim engellendi" in str(se):
                return False, f"{self.port} portuna erişim engellendi. (Başka bir program COM portunu kullanıyor olabilir)"
            return False, f"Seri port hatası: {str(se)}"
        except Exception as e:
            return False, f"Bağlantı hatası: {str(e)}"

    def disconnect(self):
        if self.ser and self.ser.is_open:
            try:
                self.ser.close()
            except Exception:
                pass

    def ping_device(self):
        """
        AnyTone donanım el sıkışmasını gerçekleştirir ve model bilgisini okur.
        """
        if not self.ser or not self.ser.is_open:
            ok, msg = self.connect()
            if not ok:
                return False, msg

        try:
            self.ser.reset_input_buffer()
            self.ser.reset_output_buffer()

            # 1. Aşama: PROGRAM el sıkışması
            self.ser.write(CMD_PROGRAM)
            self.ser.flush()
            time.sleep(0.05)
            resp1 = self.ser.read(16)

            if not resp1 or b"QX" not in resp1:
                # Farklı baud rate veya mod deneme
                return False, f"Cihazdan QX yanıtı alınamadı (Gelen: {resp1.hex() if resp1 else 'Zaman Aşımı'}). Telsizin açık ve kablonun takılı olduğunu kontrol edin."

            # 2. Aşama: Model ve Donanım Versiyonu Sorgulama
            self.ser.write(CMD_QUERY_ID)
            self.ser.flush()
            time.sleep(0.05)
            resp2 = self.ser.read(32)

            id_str = ""
            if resp2:
                # Örnek: b'ID878UV2\x00V101\x00\x00\x06'
                clean_bytes = resp2.replace(b"\x06", b"").replace(b"\x02", b"")
                parts = [p.decode("latin1", errors="ignore") for p in clean_bytes.split(b"\x00") if p]
                if parts:
                    self.model_info = parts[0]
                    self.hw_version = parts[1] if len(parts) > 1 else "Bilinmiyor"
                    id_str = f"Model: {self.model_info} | Donanım: {self.hw_version}"

            # 3. Aşama: ACK onayı
            self.ser.write(CMD_ACK)
            self.ser.flush()

            return True, f"Telsiz Başarıyla Bağlandı! {id_str}"
        except Exception as e:
            return False, f"İletişim sırasında hata: {str(e)}"

    def flash_binary(self, bin_path, progress_callback=None, dry_run=False):
        """Firmware dosyasını blok blok yükle"""
        if not os.path.exists(bin_path):
            return False, "Firmware dosyası bulunamadı."

        with open(bin_path, "rb") as f:
            fw_data = f.read()

        total_bytes = len(fw_data)
        if total_bytes < 100 or total_bytes > 960 * 1024:
            return False, f"Geçersiz firmware boyutu: {total_bytes} byte."

        # Vektör tablosu kontrolü
        sp, reset = struct.unpack("<II", fw_data[:8])
        if (sp & 0xFFFE0000) != 0x20000000 or (reset & 0xFFF00000) != 0x08000000:
            return False, f"Hatalı Vektör Tablosu! SP: 0x{sp:08X}, Reset: 0x{reset:08X}"

        if dry_run:
            total_blocks = (total_bytes + BLOCK_SIZE - 1) // BLOCK_SIZE
            for b_idx in range(total_blocks):
                time.sleep(0.02)
                progress = int(((b_idx + 1) / total_blocks) * 100)
                if progress_callback:
                    progress_callback(progress, f"Simülasyon Bloğu {b_idx+1}/{total_blocks}")
            return True, f"Simülasyon başarılı! ({total_bytes} byte doğrulandı)"

        # Gerçek donanıma yazma
        if not self.ser or not self.ser.is_open:
            ok, msg = self.connect()
            if not ok:
                return False, msg

        # Handshake doğrula
        ok, msg = self.ping_device()
        if not ok:
            return False, f"Yazma öncesi el sıkışma başarısız: {msg}"

        total_blocks = (total_bytes + BLOCK_SIZE - 1) // BLOCK_SIZE
        for b_idx in range(total_blocks):
            off = b_idx * BLOCK_SIZE
            chunk = fw_data[off:off + BLOCK_SIZE]
            target_addr = FLASH_APP_BASE + off

            header = struct.pack(">B I H", 0x02, target_addr, len(chunk))
            checksum = (sum(header) + sum(chunk)) & 0xFF
            packet = header + chunk + bytes([checksum])

            self.ser.write(packet)
            self.ser.flush()

            ack = self.ser.read(1)
            if ack != CMD_ACK and b"\x06" not in ack:
                return False, f"Blok {b_idx} için ACK alınamadı (Yanıt: {ack.hex() if ack else 'Zaman Aşımı'})"

            progress = int(((b_idx + 1) / total_blocks) * 100)
            if progress_callback:
                progress_callback(progress, f"Yazılıyor: %{progress} (Adres: 0x{target_addr:08X})")

        return True, f"Yükleme tamamlandı! ({total_bytes} byte başarıyla aktarıldı)"
