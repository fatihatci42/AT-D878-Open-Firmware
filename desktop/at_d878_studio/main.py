#!/usr/bin/env python3
"""
AT-D878 Studio - Local channel tools, firmware analysis and safe package checks
Designed for AnyTone AT-D878UVII Plus V4.00 (D878UV2A-BORD)
Author: Open Firmware Team / TA5THK
"""

import sys
import os
import json
import csv
import math
import itertools
import hashlib
from PySide6.QtWidgets import (
    QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
    QLabel, QPushButton, QTableWidget, QTableWidgetItem, QStackedWidget,
    QAbstractItemView,
    QStatusBar, QMessageBox, QComboBox, QFileDialog, QProgressBar,
    QTextEdit, QCheckBox, QGroupBox, QRadioButton, QButtonGroup,
    QLineEdit, QHeaderView, QFrame, QSpinBox, QSlider, QTabWidget,
    QSplitter, QGridLayout
)
from PySide6.QtCore import Qt, QThread, Signal, QTimer, QSettings, QStandardPaths
from PySide6.QtGui import QPixmap, QColor, QFont, QPainter, QBrush, QPen, QImageReader
import serial.tools.list_ports

_current_dir = os.path.dirname(os.path.abspath(__file__))
if _current_dir not in sys.path:
    sys.path.insert(0, _current_dir)

from comm.flasher import AnyToneFlasher
from comm.firmware_emulator import FirmwareEmulator, FirmwareReport
from gui.theme import MODERN_STYLE

# -----------------------------------------------------------------------------
# FLASHER THREAD
# -----------------------------------------------------------------------------
class FlasherThread(QThread):
    progress_signal = Signal(int, str)
    finished_signal = Signal(bool, str)

    def __init__(self, port, baud, filepath, dry_run=True, target="mcu"):
        super().__init__()
        self.port = port
        self.baud = baud
        self.filepath = filepath
        self.dry_run = dry_run
        self.target = target

    def run(self):
        flasher = AnyToneFlasher(self.port, self.baud)
        def cb(pct, text):
            self.progress_signal.emit(pct, text)

        try:
            ok, msg = flasher.flash_binary(
                self.filepath,
                progress_callback=cb,
                dry_run=self.dry_run,
                target=self.target,
            )
        except Exception as exc:
            ok, msg = False, f"Ön kontrol başarısız: {exc}"
        finally:
            flasher.disconnect()
        self.finished_signal.emit(ok, msg)


class HandshakeThread(QThread):
    result_signal = Signal(str, bool, str, str, str)

    def __init__(self, port, baud):
        super().__init__()
        self.port = port
        self.baud = baud

    def run(self):
        flasher = AnyToneFlasher(self.port, self.baud)
        try:
            ok, message = flasher.ping_device()
            self.result_signal.emit(
                self.port, ok, message, flasher.model_info, flasher.hw_version
            )
        except Exception as exc:
            self.result_signal.emit(self.port, False, f"Bağlantı testi başarısız: {exc}", "", "")
        finally:
            flasher.disconnect()


class FirmwareAnalysisThread(QThread):
    result_signal = Signal(object)

    def __init__(self, filepath, max_cycles=5000):
        super().__init__()
        self.filepath = filepath
        self.max_cycles = max_cycles

    def run(self):
        emulator = FirmwareEmulator()
        try:
            report = emulator.analyze_and_emulate(self.filepath, max_cycles=self.max_cycles)
        except Exception as exc:
            report = FirmwareReport()
            report.filepath = self.filepath
            report.vector_check_msg = f"Analiz hatası: {exc}"
            report.emulation_status = "Başlatılamadı"
            report.emulation_error = str(exc)
        self.result_signal.emit(report)


# -----------------------------------------------------------------------------
# LCD SCREEN WIDGET (160x128 PIXEL SIMULATOR)
# -----------------------------------------------------------------------------
class RadioLCDWidget(QWidget):
    """
    Renders an authentic AnyTone 1.77" TFT LCD (160x128) simulation
    based on official V4.00 Display Indicators.
    """
    def __init__(self, parent=None, scale=1.5):
        super().__init__(parent)
        self.scale_factor = scale
        self.setFixedSize(int(160 * scale), int(128 * scale)) # 240x192 at 1.5x
        self.setStyleSheet("background-color: #000000; border: 2px solid #334155; border-radius: 4px;")
        
        # State
        self.is_tx = False
        self.power_level = "T" # T, H, M, L
        self.vfo_a_freq = "145.500.00"
        self.vfo_a_name = "VFO-A 2M"
        self.vfo_a_mode = "FM"
        self.vfo_a_tone = "CSQ"
        self.vfo_b_freq = "433.500.00"
        self.vfo_b_name = "TG TURKIYE"
        self.vfo_b_mode = "DMR"
        self.vfo_b_tg = "TG 286"
        self.vfo_b_ts = "TS1"
        self.active_vfo = "A" # "A" or "B"
        self.battery_bars = 3
        self.battery_volts = "7.6V"
        self.gps_active = True
        self.bt_active = True
        self.rssi_level = 4 # 0..4

        # Menü Navigasyon Sistemi (AnyTone D878 Birebir)
        self.menu_active = False
        self.current_menu_id = "main"
        self.menu_selected = 0
        self.menu_scroll = 0
        self.menus = {
            "main": {
                "title": "ANA MENU",
                "items": [
                    {"name": "1. Telsiz Ayarlari", "type": "sub", "sub": "radio"},
                    {"name": "2. DMR Ayarlari",    "type": "sub", "sub": "dmr"},
                    {"name": "3. APRS & GPS",      "type": "sub", "sub": "aprs"},
                    {"name": "4. Bluetooth",       "type": "sub", "sub": "bt"},
                    {"name": "5. Cihaz Bilgisi",   "type": "sub", "sub": "about"},
                ]
            },
            "radio": {
                "title": "TELSIZ AYARLARI",
                "parent": "main",
                "items": [
                    {"name": "TX Gucu",      "type": "val", "values": ["TURBO", "HIGH", "MID", "LOW"], "val_idx": 0},
                    {"name": "Squelch",      "type": "val", "values": ["0", "1", "2", "3", "4", "5", "6", "7", "8"], "val_idx": 3},
                    {"name": "Tus Sesi",     "type": "val", "values": ["[ACIK]", "[KPLI]"], "val_idx": 0},
                    {"name": "Ekran Isigi",  "type": "val", "values": ["10 s", "30 s", "SUREKLI"], "val_idx": 0},
                    {"name": "Parlaklik",    "type": "val", "values": ["1", "2", "3", "4", "5"], "val_idx": 3},
                    {"name": "Dil Secimi",   "type": "val", "values": ["TURKCE", "ENGLISH"], "val_idx": 0},
                ]
            },
            "dmr": {
                "title": "DMR AYARLARI",
                "parent": "main",
                "items": [
                    {"name": "Renk Kodu",    "type": "val", "values": ["CC 1", "CC 2", "CC 3"], "val_idx": 0},
                    {"name": "Zaman Slotu",  "type": "val", "values": ["Slot 1", "Slot 2"], "val_idx": 0},
                    {"name": "Grup (TG)",    "type": "val", "values": ["TG 286 (TR)", "TG 91 (WW)", "TG 9 (Local)"], "val_idx": 0},
                    {"name": "Radio ID",     "type": "val", "values": ["2860501"], "val_idx": 0},
                ]
            },
            "aprs": {
                "title": "APRS & GPS",
                "parent": "main",
                "items": [
                    {"name": "Konum Gonder", "type": "action", "val": "[GONDER]"},
                    {"name": "GPS Durumu",   "type": "val", "values": ["[AKTIF]", "[KAPALI]"], "val_idx": 0},
                    {"name": "Frekans",      "type": "val", "values": ["144.800 MHz"], "val_idx": 0},
                    {"name": "Cagri Kodu",   "type": "val", "values": ["TA5THK-7"], "val_idx": 0},
                ]
            },
            "bt": {
                "title": "BLUETOOTH",
                "parent": "main",
                "items": [
                    {"name": "BT Gucu",      "type": "val", "values": ["[KPLI]", "[ACIK]"], "val_idx": 0},
                    {"name": "Baglanti",     "type": "val", "values": ["Bos"], "val_idx": 0},
                ]
            },
            "about": {
                "title": "CIHAZ BILGISI",
                "parent": "main",
                "items": [
                    {"name": "Model",        "type": "val", "values": ["D878UVII+"], "val_idx": 0},
                    {"name": "Firmware",     "type": "val", "values": ["Open v0.1"], "val_idx": 0},
                    {"name": "MCU",          "type": "val", "values": ["GD32F303"], "val_idx": 0},
                    {"name": "Saat",         "type": "val", "values": ["120 MHz"], "val_idx": 0},
                    {"name": "Cagri",        "type": "val", "values": ["TA5THK"], "val_idx": 0},
                ]
            }
        }

    def menu_up(self):
        if not self.menu_active:
            return
        m = self.menus[self.current_menu_id]
        if self.menu_selected > 0:
            self.menu_selected -= 1
            if self.menu_selected < self.menu_scroll:
                self.menu_scroll = self.menu_selected
        else:
            self.menu_selected = len(m["items"]) - 1
            if len(m["items"]) > 5:
                self.menu_scroll = len(m["items"]) - 5
        self.update()

    def menu_down(self):
        if not self.menu_active:
            return
        m = self.menus[self.current_menu_id]
        if self.menu_selected < len(m["items"]) - 1:
            self.menu_selected += 1
            if self.menu_selected >= self.menu_scroll + 5:
                self.menu_scroll = self.menu_selected - 4
        else:
            self.menu_selected = 0
            self.menu_scroll = 0
        self.update()

    def menu_enter(self):
        if not self.menu_active:
            self.menu_active = True
            self.current_menu_id = "main"
            self.menu_selected = 0
            self.menu_scroll = 0
            self.update()
            return
        m = self.menus[self.current_menu_id]
        item = m["items"][self.menu_selected]
        if item.get("type") == "sub":
            self.current_menu_id = item["sub"]
            self.menu_selected = 0
            self.menu_scroll = 0
        elif item.get("type") == "val":
            vals = item["values"]
            item["val_idx"] = (item["val_idx"] + 1) % len(vals)
        self.update()

    def menu_back(self):
        if not self.menu_active:
            return
        m = self.menus[self.current_menu_id]
        parent = m.get("parent")
        if parent:
            self.current_menu_id = parent
            self.menu_selected = 0
            self.menu_scroll = 0
        else:
            self.menu_active = False
        self.update()

    def menu_toggle(self):
        self.menu_active = not self.menu_active
        if self.menu_active:
            self.current_menu_id = "main"
            self.menu_selected = 0
            self.menu_scroll = 0
        self.update()

    def key_digit(self, digit_str):
        if self.menu_active:
            try:
                num = int(digit_str)
                m = self.menus[self.current_menu_id]
                if 1 <= num <= len(m["items"]):
                    self.menu_selected = num - 1
                    self.update()
            except ValueError:
                pass
        else:
            if not hasattr(self, 'freq_buffer'):
                self.freq_buffer = ""
            if digit_str in "0123456789":
                self.freq_buffer += digit_str
                if len(self.freq_buffer) > 8:
                    self.freq_buffer = digit_str
                f_padded = self.freq_buffer.ljust(8, '0')
                formatted = f"{f_padded[:3]}.{f_padded[3:6]}.{f_padded[6:8]}"
                if self.active_vfo == "A":
                    self.vfo_a_freq = formatted
                else:
                    self.vfo_b_freq = formatted
                self.update()

    def key_star(self):
        self.freq_buffer = ""
        self.update()

    def key_hash(self):
        if self.active_vfo == "A":
            self.vfo_a_mode = "DMR" if self.vfo_a_mode == "FM" else "FM"
        else:
            self.vfo_b_mode = "DMR" if self.vfo_b_mode == "FM" else "FM"
        self.update()

    def step_freq(self, up=True):
        if self.menu_active:
            if up:
                self.menu_up()
            else:
                self.menu_down()
            return
        attr = "vfo_a_freq" if self.active_vfo == "A" else "vfo_b_freq"
        cur_str = getattr(self, attr).replace(".", "")
        try:
            val = int(cur_str)
            step = 1250 # 12.5 kHz
            val = val + step if up else val - step
            if val < 13400000: val = 13400000
            if val > 52000000: val = 52000000
            val_str = f"{val:08d}"
            setattr(self, attr, f"{val_str[:3]}.{val_str[3:6]}.{val_str[6:8]}")
            self.update()
        except ValueError:
            pass

    def toggle_vfo(self):
        self.active_vfo = "B" if self.active_vfo == "A" else "A"
        self.update()

    def toggle_power(self):
        p_seq = ["T", "H", "M", "L"]
        cur = self.power_level
        nxt = p_seq[(p_seq.index(cur) + 1) % len(p_seq)]
        self.power_level = nxt
        self.update()

    def paintEvent(self, event):
        painter = QPainter(self)
        painter.scale(self.scale_factor, self.scale_factor)
        
        # Arka plan
        painter.fillRect(0, 0, 160, 128, QColor("#000000"))

        # EĞER MENÜ AKTİFSE: Menü Ekranını Çiz
        if self.menu_active:
            m = self.menus[self.current_menu_id]
            # 1. Üst Başlık Çubuğu
            painter.fillRect(0, 0, 160, 16, QColor("#0F172A"))
            painter.setPen(QColor("#FBBF24"))
            painter.setFont(QFont("Segoe UI", 7, QFont.Bold))
            painter.drawText(6, 12, m["title"])

            painter.setPen(QColor("#FFFFFF"))
            painter.setFont(QFont("Segoe UI", 6))
            page_info = f"{self.menu_selected + 1}/{len(m['items'])}"
            painter.drawText(130, 12, page_info)

            # 2. Menü Satırları (18..112)
            visible_count = min(5, len(m["items"]) - self.menu_scroll)
            for i in range(visible_count):
                idx = self.menu_scroll + i
                item = m["items"][idx]
                is_sel = (idx == self.menu_selected)

                row_y = 18 + (i * 19)
                row_bg = QColor("#09182B") if is_sel else QColor("#050A12")
                painter.fillRect(0, row_y, 160, 18, row_bg)
                if is_sel:
                    painter.setPen(QPen(QColor("#1E3A5F"), 1))
                    painter.drawRect(0, row_y, 159, 18)

                # İmleç
                if is_sel:
                    painter.setPen(QColor("#FBBF24"))
                    painter.setFont(QFont("Segoe UI", 7, QFont.Bold))
                    painter.drawText(4, row_y + 13, "▶")

                # Metin
                painter.setPen(QColor("#FFFFFF") if is_sel else QColor("#94A3B8"))
                painter.setFont(QFont("Segoe UI", 6, QFont.Bold if is_sel else QFont.Normal))
                painter.drawText(14, row_y + 13, item["name"])

                # Değer
                val_text = ""
                if item.get("type") == "sub":
                    val_text = ">>"
                elif item.get("type") == "val":
                    vals = item["values"]
                    v_idx = item.get("val_idx", 0)
                    val_text = vals[v_idx]
                elif item.get("type") == "action":
                    val_text = item.get("val", "")

                if val_text:
                    painter.setPen(QColor("#FBBF24") if is_sel else QColor("#38BDF8"))
                    painter.setFont(QFont("Segoe UI", 6, QFont.Bold))
                    painter.drawText(106, row_y + 13, val_text)

            # 3. Alt Yumuşak Tuşlar
            painter.fillRect(0, 114, 160, 14, QColor("#1E293B"))
            painter.setPen(QColor("#10B981"))
            painter.setFont(QFont("Segoe UI", 6, QFont.Bold))
            painter.drawText(8, 124, "[SEC]")
            painter.setPen(QColor("#EF4444"))
            painter.drawText(125, 124, "[GERI]")
            return

        # 1. ÜST DURUM ÇUBUĞU (Status Bar 0..14)
        painter.fillRect(0, 0, 160, 14, QColor("#0F172A"))
        painter.setPen(QPen(QColor("#334155"), 1))
        painter.drawLine(0, 14, 160, 14)

        # S-Metre / Anten (Sol)
        painter.setPen(QColor("#38BDF8"))
        painter.setFont(QFont("Consolas", 6, QFont.Bold))
        painter.drawText(2, 10, "Y") # Anten
        for i in range(4):
            bar_h = (i + 1) * 2
            col = QColor("#10B981") if i < self.rssi_level else QColor("#334155")
            painter.fillRect(7 + (i * 3), 11 - bar_h, 2, bar_h, col)

        # Güç Seviyesi (T, H, M, L)
        painter.setPen(QColor("#F59E0B"))
        painter.drawText(22, 10, self.power_level)

        # Monitör Hoparlör
        painter.setPen(QColor("#94A3B8"))
        painter.drawText(31, 10, "🔊")

        # GPS Uydu
        painter.setPen(QColor("#10B981") if self.gps_active else QColor("#64748B"))
        painter.drawText(46, 10, "GPS")

        # Bluetooth
        painter.setPen(QColor("#38BDF8") if self.bt_active else QColor("#64748B"))
        painter.drawText(68, 10, "B")

        # DMR Color Code
        painter.setPen(QColor("#A78BFA"))
        painter.drawText(80, 10, "C1")

        # Pil & Voltaj
        painter.setPen(QColor("#94A3B8"))
        painter.setFont(QFont("Consolas", 5))
        painter.drawText(112, 10, self.battery_volts)
        # Pil Çerçevesi
        painter.setPen(QColor("#E2E8F0"))
        painter.drawRect(140, 3, 14, 7)
        painter.fillRect(154, 5, 2, 3, QColor("#E2E8F0"))
        # Pil Barları
        for i in range(self.battery_bars):
            painter.fillRect(142 + (i * 4), 5, 3, 4, QColor("#10B981"))

        # 2. VFO A KUTUSU (15..66)
        is_a = (self.active_vfo == "A")
        bg_a = QColor("#09182B") if is_a else QColor("#050A12")
        painter.fillRect(0, 15, 160, 50, bg_a)
        painter.setPen(QPen(QColor("#1E3A5F") if is_a else QColor("#0F172A"), 1))
        painter.drawRect(0, 15, 159, 50)

        # Aktif İmleç
        if is_a:
            painter.setPen(QColor("#FBBF24"))
            painter.setFont(QFont("Segoe UI", 7, QFont.Bold))
            painter.drawText(3, 26, "▶")

        # Kanal Adı ve Mod
        painter.setPen(QColor("#94A3B8"))
        painter.setFont(QFont("Segoe UI", 6, QFont.Bold))
        painter.drawText(14, 26, self.vfo_a_name)
        
        mode_a_col = QColor("#38BDF8") if self.vfo_a_mode == "FM" else QColor("#A78BFA")
        painter.setPen(mode_a_col)
        painter.drawText(130, 26, f"[{self.vfo_a_mode}]")

        # Ana Frekans Metni (Büyük)
        painter.setPen(QColor("#FFFFFF") if not self.is_tx else QColor("#EF4444"))
        painter.setFont(QFont("Consolas", 12, QFont.Bold))
        painter.drawText(14, 44, self.vfo_a_freq)

        # Alt Bilgi (Ton / Zone)
        painter.setPen(QColor("#64748B"))
        painter.setFont(QFont("Segoe UI", 6))
        painter.drawText(14, 58, f"ZONE: TURKIYE | {self.vfo_a_tone}")

        # 3. VFO B KUTUSU (67..113)
        is_b = (self.active_vfo == "B")
        bg_b = QColor("#09182B") if is_b else QColor("#050A12")
        painter.fillRect(0, 66, 160, 48, bg_b)
        painter.setPen(QPen(QColor("#1E3A5F") if is_b else QColor("#0F172A"), 1))
        painter.drawRect(0, 66, 159, 48)

        if is_b:
            painter.setPen(QColor("#FBBF24"))
            painter.setFont(QFont("Segoe UI", 7, QFont.Bold))
            painter.drawText(3, 77, "▶")

        painter.setPen(QColor("#94A3B8"))
        painter.setFont(QFont("Segoe UI", 6, QFont.Bold))
        painter.drawText(14, 77, self.vfo_b_name)

        mode_b_col = QColor("#38BDF8") if self.vfo_b_mode == "FM" else QColor("#A78BFA")
        painter.setPen(mode_b_col)
        painter.drawText(130, 77, f"[{self.vfo_b_mode}]")

        painter.setPen(QColor("#CBD5E1") if not (self.is_tx and is_b) else QColor("#EF4444"))
        painter.setFont(QFont("Consolas", 12, QFont.Bold))
        painter.drawText(14, 94, self.vfo_b_freq)

        painter.setPen(QColor("#64748B"))
        painter.setFont(QFont("Segoe UI", 6))
        painter.drawText(14, 107, f"{self.vfo_b_tg} | {self.vfo_b_ts} | CC: 1")

        # 4. ALT YUMUŞAK TUŞLAR (114..128)
        painter.fillRect(0, 114, 160, 14, QColor("#1E293B") if not self.is_tx else QColor("#7F1D1D"))
        painter.setPen(QColor("#FFFFFF"))
        painter.setFont(QFont("Segoe UI", 6, QFont.Bold))
        if self.is_tx:
            painter.setPen(QColor("#FCA5A5"))
            painter.drawText(50, 125, "● TX SIM")
        else:
            painter.drawText(10, 124, "Menü")
            painter.drawText(68, 124, "Bölge")
            painter.drawText(125, 124, "Tara")


# -----------------------------------------------------------------------------
# ANYTONE FİZİKSEL KASA & ETKİLEŞİMLİ ŞABLON WIDGET'I
# -----------------------------------------------------------------------------
class AnyToneHandsetWidget(QFrame):
    """
    Kullanıcının paylaştığı fiziksel AnyTone AT-D878UVII Plus şablonunun birebir
    interaktif simülatör karşılığı:
      - Üst: Anten, Döner Kanal Düğmesi (CH), Ses/Güç Düğmesi (VOL), PF3 Yeşil Tepe Tuşu
      - Sol Yan: Taktik Mandallı PTT Tuşu, PF1 ve PF2 Yan Tuşları
      - Ön Panel:
        * AnyTone Metalik Logo
        * 1.77" Renkli TFT LCD (160x128 RGB565)
        * Fonksiyon Tuşları: [-] MENU, 4 Yönlü D-Pad Halka Tuşu (▲ ▼ ◀ ▶ OK), [--] EXIT
        * P1 ve P2 Programlanabilir Tuşları
        * Akustik Hoparlör Izgarası
        * 12 Tuşlu Alfanumerik Keypad (1-9, *, 0, #)
    """
    def __init__(self, lcd: RadioLCDWidget, parent=None):
        super().__init__(parent)
        self.lcd = lcd
        self.setFixedWidth(340)
        self.setStyleSheet("""
            QFrame#HandsetOuter {
                background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #11151C, stop:0.04 #1E2430, stop:0.96 #1E2430, stop:1 #11151C);
                border: 2px solid #334155;
                border-radius: 24px;
            }
        """)
        self.setObjectName("HandsetOuter")
        self.init_ui()

    def init_ui(self):
        main_layout = QVBoxLayout(self)
        main_layout.setContentsMargins(12, 8, 12, 14)
        main_layout.setSpacing(6)

        # 1. ÜST BÖLÜM: ANTEN, KANAL VE SES DÜĞMELERİ, PF3 TEPE TUŞU
        top_bar = QHBoxLayout()
        top_bar.setSpacing(6)

        # Anten
        lbl_antenna = QLabel("║")
        lbl_antenna.setToolTip("AnyTone VHF/UHF Helical Anten")
        lbl_antenna.setStyleSheet("""
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #0F172A, stop:0.5 #334155, stop:1 #0F172A);
            color: #94A3B8; font-size: 14px; font-weight: 900;
            border: 1px solid #1E293B; border-top-left-radius: 6px; border-top-right-radius: 6px;
            padding: 4px;
        """)
        lbl_antenna.setAlignment(Qt.AlignCenter)
        lbl_antenna.setFixedSize(24, 38)
        top_bar.addWidget(lbl_antenna)

        # Kanal Döner Düğmesi (Rotary Encoder)
        ch_box = QHBoxLayout()
        ch_box.setSpacing(1)
        btn_ch_dec = QPushButton("◀")
        btn_ch_dec.setToolTip("Kanal/Frekans Azalt (Encoder Sol)")
        btn_ch_dec.setFixedSize(22, 28)
        btn_ch_dec.setStyleSheet("background: #0F172A; border: 1px solid #334155; border-radius: 4px; color: #94A3B8; font-size: 10px; font-weight: 700;")
        btn_ch_dec.clicked.connect(lambda: self.lcd.step_freq(False))

        lbl_ch = QLabel("CH\nENCD")
        lbl_ch.setAlignment(Qt.AlignCenter)
        lbl_ch.setFixedSize(34, 36)
        lbl_ch.setStyleSheet("background: #0B0F17; border: 2px solid #475569; border-radius: 6px; color: #38BDF8; font-size: 8px; font-weight: 800;")

        btn_ch_inc = QPushButton("▶")
        btn_ch_inc.setToolTip("Kanal/Frekans Artır (Encoder Sağ)")
        btn_ch_inc.setFixedSize(22, 28)
        btn_ch_inc.setStyleSheet("background: #0F172A; border: 1px solid #334155; border-radius: 4px; color: #94A3B8; font-size: 10px; font-weight: 700;")
        btn_ch_inc.clicked.connect(lambda: self.lcd.step_freq(True))

        ch_box.addWidget(btn_ch_dec)
        ch_box.addWidget(lbl_ch)
        ch_box.addWidget(btn_ch_inc)
        top_bar.addLayout(ch_box)

        # Ses / Güç Düğmesi
        lbl_vol = QLabel("VOL\nPOW")
        lbl_vol.setAlignment(Qt.AlignCenter)
        lbl_vol.setFixedSize(36, 36)
        lbl_vol.setStyleSheet("background: #0B0F17; border: 2px solid #475569; border-radius: 6px; color: #F59E0B; font-size: 8px; font-weight: 800;")
        top_bar.addWidget(lbl_vol)

        # PF3 Tepe Yeşil Tuşu
        btn_pf3 = QPushButton("PF3")
        btn_pf3.setToolTip("PF3 yerel önizleme; RF yayını yapılmaz")
        btn_pf3.setFixedSize(34, 36)
        btn_pf3.setStyleSheet("""
            QPushButton {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #10B981, stop:1 #047857);
                border: 2px solid #065F46; border-radius: 6px;
                color: white; font-size: 9px; font-weight: 900;
            }
            QPushButton:hover { background: #059669; }
            QPushButton:pressed { background: #064E3B; }
        """)
        btn_pf3.clicked.connect(self.on_pf3_click)
        top_bar.addWidget(btn_pf3)

        main_layout.addLayout(top_bar)

        # 2. GÖVDE: SOL YAN TUŞLAR (PTT / PF1 / PF2) + ORTA ÖN PANEL
        body_h = QHBoxLayout()
        body_h.setSpacing(6)

        # Sol Yan Tuşlar Paneli
        side_v = QVBoxLayout()
        side_v.setSpacing(4)
        side_v.addSpacing(28)

        # PTT Tuşu
        self.btn_ptt = QPushButton("P\nT\nT")
        self.btn_ptt.setToolTip("PTT (Mandal): Basılı tutarak konuş / iletime geç")
        self.btn_ptt.setFixedSize(28, 90)
        self.style_ptt_normal = """
            QPushButton {
                background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #7F1D1D, stop:0.4 #DC2626, stop:1 #7F1D1D);
                border: 2px solid #991B1B; border-radius: 6px;
                color: white; font-size: 11px; font-weight: 900;
            }
            QPushButton:hover { background: #EF4444; }
        """
        self.style_ptt_active = """
            QPushButton {
                background: #F87171; border: 2px solid #FFFFFF; border-radius: 6px;
                color: #000; font-size: 11px; font-weight: 900;
            }
        """
        self.btn_ptt.setStyleSheet(self.style_ptt_normal)
        self.btn_ptt.pressed.connect(self.on_ptt_pressed)
        self.btn_ptt.released.connect(self.on_ptt_released)
        side_v.addWidget(self.btn_ptt)

        # PF1 Tuşu
        btn_pf1 = QPushButton("PF1")
        btn_pf1.setToolTip("PF1: Monitör / Squelch Geçici Aç")
        btn_pf1.setFixedSize(28, 30)
        btn_pf1.setStyleSheet("background: #1E293B; border: 1px solid #475569; border-radius: 4px; color: #CBD5E1; font-size: 8px; font-weight: 700;")
        btn_pf1.clicked.connect(lambda: self.lcd.step_freq(True))
        side_v.addWidget(btn_pf1)

        # PF2 Tuşu
        btn_pf2 = QPushButton("PF2")
        btn_pf2.setToolTip("PF2: Gerilim Oku / Ayar")
        btn_pf2.setFixedSize(28, 30)
        btn_pf2.setStyleSheet("background: #1E293B; border: 1px solid #475569; border-radius: 4px; color: #CBD5E1; font-size: 8px; font-weight: 700;")
        btn_pf2.clicked.connect(lambda: self.lcd.step_freq(False))
        side_v.addWidget(btn_pf2)

        side_v.addStretch()
        body_h.addLayout(side_v)

        # Orta Panel (Ekran, D-Pad, Tuş Takımı)
        front_v = QVBoxLayout()
        front_v.setSpacing(4)
        front_v.setContentsMargins(0, 0, 0, 0)

        # AnyTone Logo
        lbl_logo = QLabel("<i>AnyTone</i>")
        lbl_logo.setAlignment(Qt.AlignCenter)
        lbl_logo.setStyleSheet("color: #E2E8F0; font-size: 14px; font-weight: 900; letter-spacing: 2px;")
        front_v.addWidget(lbl_logo)

        # LCD Ekran Çerçevesi
        screen_frame = QFrame()
        screen_frame.setStyleSheet("background: #000000; border: 2px solid #0F172A; border-radius: 6px; padding: 2px;")
        sf_lay = QVBoxLayout(screen_frame)
        sf_lay.setContentsMargins(0, 0, 0, 0)
        sf_lay.addWidget(self.lcd, alignment=Qt.AlignCenter)
        front_v.addWidget(screen_frame, alignment=Qt.AlignCenter)

        # 3. FONKSİYON TUŞLARI & D-PAD NAVİGASYON HALKASI (Şablondaki birebir yerleşim)
        # Sol: [-] MENU | Orta: [▲ ▼ ◀ ▶ OK] | Sağ: [--] EXIT
        fn_row = QHBoxLayout()
        fn_row.setSpacing(4)

        # [-] Menu Tuşu (AnyTone Tek Çizgi)
        self.btn_menu = QPushButton("—\nMENU")
        self.btn_menu.setToolTip("Menü / Seç / Onayla Tuşu")
        self.btn_menu.setFixedSize(54, 46)
        self.btn_menu.setStyleSheet("""
            QPushButton {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #1E293B, stop:1 #0F172A);
                border: 2px solid #059669; border-radius: 8px;
                color: #10B981; font-size: 9px; font-weight: 900;
            }
            QPushButton:hover { background: #064E3B; color: white; border-color: #10B981; }
            QPushButton:pressed { background: #10B981; color: black; }
        """)
        self.btn_menu.clicked.connect(self.lcd.menu_enter)
        fn_row.addWidget(self.btn_menu)

        # Orta D-Pad Halka Navigasyon Grubu
        dpad_grid = QGridLayout()
        dpad_grid.setSpacing(2)

        btn_up = QPushButton("▲")
        btn_up.setToolTip("Yukarı / Frekans Artır")
        btn_up.setFixedSize(28, 20)
        btn_up.setStyleSheet("background: #1E293B; border: 1px solid #475569; border-radius: 4px; color: #FBBF24; font-size: 9px; font-weight: 700;")
        btn_up.clicked.connect(self.on_nav_up)
        dpad_grid.addWidget(btn_up, 0, 1)

        btn_left = QPushButton("◀")
        btn_left.setToolTip("Sol / VFO-A")
        btn_left.setFixedSize(26, 20)
        btn_left.setStyleSheet("background: #1E293B; border: 1px solid #475569; border-radius: 4px; color: #38BDF8; font-size: 9px; font-weight: 700;")
        btn_left.clicked.connect(self.on_nav_left)
        dpad_grid.addWidget(btn_left, 1, 0)

        btn_ok = QPushButton("OK")
        btn_ok.setToolTip("Tamam / Menü Aç / Gir")
        btn_ok.setFixedSize(28, 20)
        btn_ok.setStyleSheet("background: #0284C7; border: 1px solid #38BDF8; border-radius: 4px; color: white; font-size: 8px; font-weight: 900;")
        btn_ok.clicked.connect(self.lcd.menu_enter)
        dpad_grid.addWidget(btn_ok, 1, 1)

        btn_right = QPushButton("▶")
        btn_right.setToolTip("Sağ / VFO-B")
        btn_right.setFixedSize(26, 20)
        btn_right.setStyleSheet("background: #1E293B; border: 1px solid #475569; border-radius: 4px; color: #38BDF8; font-size: 9px; font-weight: 700;")
        btn_right.clicked.connect(self.on_nav_right)
        dpad_grid.addWidget(btn_right, 1, 2)

        btn_down = QPushButton("▼")
        btn_down.setToolTip("Aşağı / Frekans Azalt")
        btn_down.setFixedSize(28, 20)
        btn_down.setStyleSheet("background: #1E293B; border: 1px solid #475569; border-radius: 4px; color: #FBBF24; font-size: 9px; font-weight: 700;")
        btn_down.clicked.connect(self.on_nav_down)
        dpad_grid.addWidget(btn_down, 2, 1)

        fn_row.addLayout(dpad_grid)

        # [--] Exit Tuşu (AnyTone Çift Çizgi)
        self.btn_exit = QPushButton("——\nEXIT")
        self.btn_exit.setToolTip("Çıkış / Geri / İptal Tuşu")
        self.btn_exit.setFixedSize(54, 46)
        self.btn_exit.setStyleSheet("""
            QPushButton {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #1E293B, stop:1 #0F172A);
                border: 2px solid #DC2626; border-radius: 8px;
                color: #EF4444; font-size: 9px; font-weight: 900;
            }
            QPushButton:hover { background: #7F1D1D; color: white; border-color: #EF4444; }
            QPushButton:pressed { background: #DC2626; color: white; }
        """)
        self.btn_exit.clicked.connect(self.lcd.menu_back)
        fn_row.addWidget(self.btn_exit)

        front_v.addLayout(fn_row)

        # 4. P1 ve P2 PROGRAMLANABİLİR TUŞLAR
        p_row = QHBoxLayout()
        p_row.setSpacing(6)

        btn_p1 = QPushButton("P1 (VFO)")
        btn_p1.setToolTip("P1: VFO-A / VFO-B Geçişi Yap")
        btn_p1.setFixedSize(110, 24)
        btn_p1.setStyleSheet("background: #1E2530; border: 1px solid #475569; border-radius: 4px; color: #E2E8F0; font-size: 9px; font-weight: 700;")
        btn_p1.clicked.connect(self.lcd.toggle_vfo)
        p_row.addWidget(btn_p1)

        btn_p2 = QPushButton("P2 (GÜÇ)")
        btn_p2.setToolTip("P2: TX Çıkış Gücünü Değiştir (Turbo/High/Mid/Low)")
        btn_p2.setFixedSize(110, 24)
        btn_p2.setStyleSheet("background: #1E2530; border: 1px solid #475569; border-radius: 4px; color: #E2E8F0; font-size: 9px; font-weight: 700;")
        btn_p2.clicked.connect(self.lcd.toggle_power)
        p_row.addWidget(btn_p2)

        front_v.addLayout(p_row)

        # 5. AKUSTİK HOPARLÖR IZGARASI
        lbl_grille = QLabel("••••••••••    ••••••••••")
        lbl_grille.setAlignment(Qt.AlignCenter)
        lbl_grille.setStyleSheet("color: #334155; font-size: 10px; font-weight: 900; letter-spacing: 3px; margin: 2px 0;")
        front_v.addWidget(lbl_grille)

        # 6. 12 TUŞLU ALFANUMERİK KEYPAD (Şablondaki 4x3 yerleşim)
        key_grid = QGridLayout()
        key_grid.setSpacing(3)

        keys_data = [
            ("1 !<",   "1", 0, 0), ("2 ABC",  "2", 0, 1), ("3 DEF",  "3", 0, 2),
            ("4 GHI",  "4", 1, 0), ("5 JKL",  "5", 1, 1), ("6 MNO",  "6", 1, 2),
            ("7 PQRS", "7", 2, 0), ("8 TUV",  "8", 2, 1), ("9 WXYZ", "9", 2, 2),
            ("* ␣",    "*", 3, 0), ("0 +",    "0", 3, 1), ("# ↑",    "#", 3, 2),
        ]

        for label, val, r, c in keys_data:
            btn = QPushButton(label)
            btn.setFixedSize(70, 27)
            btn.setStyleSheet("""
                QPushButton {
                    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #1E2530, stop:1 #11151C);
                    border: 1px solid #334155; border-radius: 5px;
                    color: #F8FAFC; font-size: 8px; font-weight: 700;
                }
                QPushButton:hover { background: #334155; border-color: #64748B; }
                QPushButton:pressed { background: #0284C7; color: white; }
            """)
            if val in "0123456789":
                btn.clicked.connect(lambda _, d=val: self.lcd.key_digit(d))
            elif val == "*":
                btn.clicked.connect(self.lcd.key_star)
            elif val == "#":
                btn.clicked.connect(self.lcd.key_hash)
            key_grid.addWidget(btn, r, c)

        front_v.addLayout(key_grid)
        body_h.addLayout(front_v)
        main_layout.addLayout(body_h)

    def on_ptt_pressed(self):
        self.btn_ptt.setStyleSheet(self.style_ptt_active)
        self.lcd.is_tx = True
        self.lcd.update()

    def on_ptt_released(self):
        self.btn_ptt.setStyleSheet(self.style_ptt_normal)
        self.lcd.is_tx = False
        self.lcd.update()

    def on_nav_up(self):
        if self.lcd.menu_active:
            self.lcd.menu_up()
        else:
            self.lcd.step_freq(True)

    def on_nav_down(self):
        if self.lcd.menu_active:
            self.lcd.menu_down()
        else:
            self.lcd.step_freq(False)

    def on_nav_left(self):
        if self.lcd.menu_active:
            self.lcd.menu_back()
        else:
            self.lcd.active_vfo = "A"
            self.lcd.update()

    def on_nav_right(self):
        if self.lcd.menu_active:
            self.lcd.menu_enter()
        else:
            self.lcd.active_vfo = "B"
            self.lcd.update()

    def on_pf3_click(self):
        self.window().statusBar().showMessage("PF3 maket eylemi: APRS/RF gönderimi yapılmadı.", 4000)


# -----------------------------------------------------------------------------
# ANA PENCERE (MAIN WINDOW)
# -----------------------------------------------------------------------------
class ATD878Studio(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("AT-D878 Studio | Kanal, DMR ve firmware araçları v0.4.0")
        self.resize(1260, 840)
        self.setMinimumSize(1020, 700)

        self.settings = QSettings("AnyTone", "AT-D878 Studio")
        self.workspace_dir = QStandardPaths.writableLocation(QStandardPaths.AppLocalDataLocation)
        self.workspace_path = os.path.join(self.workspace_dir, "workspace.json")
        self.workspace_data = self._read_workspace()
        self.handshake_thread = None
        self.flash_thread = None
        self.analysis_thread = None
        self.project_root = self._find_project_root()
        self.release_dir = os.path.join(self.project_root, "tools", "firmware_analyzer", "D878UVII PLUS V4.00 official release 250815")
        
        self.setStyleSheet(MODERN_STYLE)
        self.init_ui()
        self.autosave_timer = QTimer(self)
        self.autosave_timer.setSingleShot(True)
        self.autosave_timer.setInterval(700)
        self.autosave_timer.timeout.connect(self._persist_workspace)
        self.table_channels.itemChanged.connect(self._schedule_workspace_save)
        self.table_contacts.itemChanged.connect(self._schedule_workspace_save)
        self.port_combo.currentIndexChanged.connect(self._save_last_port)
        self.persistent_controls = self._persistent_control_items()
        self._restore_persistent_controls()
        for _key, control in self.persistent_controls:
            if isinstance(control, QComboBox):
                control.currentIndexChanged.connect(self._schedule_workspace_save)
            elif isinstance(control, QLineEdit):
                control.textChanged.connect(self._schedule_workspace_save)
            elif isinstance(control, QCheckBox):
                control.toggled.connect(self._schedule_workspace_save)
            elif isinstance(control, QSpinBox):
                control.valueChanged.connect(self._schedule_workspace_save)
        saved_geometry = self.settings.value("window/geometry")
        if saved_geometry:
            self.restoreGeometry(saved_geometry)
        self._restore_last_port()
        if not self._restore_table("data/channels", self.table_channels):
            self.load_empty_channels()
        if not self._restore_table("data/contacts", self.table_contacts):
            self.load_sample_contacts()

    @staticmethod
    def _find_project_root():
        if getattr(sys, "frozen", False):
            executable_dir = os.path.dirname(os.path.abspath(sys.executable))
            candidates = (
                getattr(sys, "_MEIPASS", ""),
                executable_dir,
                os.path.abspath(os.path.join(executable_dir, "..")),
            )
            for candidate in candidates:
                if candidate and os.path.isdir(candidate) and (
                    os.path.isdir(os.path.join(candidate, "tools"))
                    or os.path.isdir(os.path.join(candidate, "firmware"))
                ):
                    return os.path.abspath(candidate)
            return executable_dir
        candidates = (os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..")),)
        for candidate in candidates:
            if candidate and os.path.isdir(candidate) and (
                os.path.isdir(os.path.join(candidate, "tools"))
                or os.path.isdir(os.path.join(candidate, "firmware"))
            ):
                return os.path.abspath(candidate)
        return os.path.abspath(candidates[-1])

    def _restore_last_port(self):
        saved_port = self.settings.value("serial/last_port", "")
        if not saved_port:
            return
        index = self.port_combo.findData(saved_port)
        if index >= 0:
            self.port_combo.setCurrentIndex(index)

    def _save_last_port(self, _index=None):
        port = self.port_combo.currentData()
        if port:
            self.settings.setValue("serial/last_port", port)

    def _restore_table(self, key, table):
        rows = self.workspace_data.get(key)
        if not isinstance(rows, list):
            return False
        try:
            if not isinstance(rows, list) or len(rows) > 10000:
                return False
            table.setRowCount(len(rows))
            for row_index, row in enumerate(rows):
                if not isinstance(row, list):
                    continue
                for column, value in enumerate(row[:table.columnCount()]):
                    table.setItem(row_index, column, QTableWidgetItem(str(value or "")))
            return bool(rows)
        except (TypeError, ValueError):
            return False

    def _read_workspace(self):
        try:
            if not os.path.isfile(self.workspace_path) or os.path.getsize(self.workspace_path) > 64 * 1024 * 1024:
                return {}
            with open(self.workspace_path, "r", encoding="utf-8") as source:
                data = json.load(source)
            return data if isinstance(data, dict) else {}
        except (OSError, UnicodeError, json.JSONDecodeError):
            return {}

    def _persistent_control_items(self):
        items = [
            ("aprs/frequency", self.combo_aprs_freq),
            ("aprs/callsign", self.txt_aprs_call),
            ("aprs/path", self.txt_aprs_path),
            ("aprs/interval", self.combo_aprs_interval),
            ("aprs/symbol", self.combo_aprs_sym),
            ("aprs/comment", self.txt_aprs_comment),
            ("aprs/dmr_slot", self.combo_dmr_ts),
            ("aprs/dmr_tg", self.txt_dmr_aprs_tg),
            ("aprs/roaming_enabled", self.chk_roam_auto),
            ("aprs/roaming_distance", self.spin_roam_dist),
            ("radio/mic_analog", self.spin_mic_a),
            ("radio/mic_digital", self.spin_mic_d),
            ("radio/backlight", self.combo_light),
            ("radio/dim_shut", self.chk_dim_shut),
            ("radio/boot_message", self.txt_boot_msg),
        ]
        items.extend((f"radio/key_assignment_{index}", control) for index, control in enumerate(self.key_assignment_combos))
        return items

    def _restore_persistent_controls(self):
        saved = self.workspace_data.get("controls", {})
        if not isinstance(saved, dict):
            return
        for key, control in self._persistent_control_items():
            if key not in saved:
                continue
            value = saved[key]
            if isinstance(control, QComboBox):
                index = control.findText(str(value))
                if index >= 0:
                    control.setCurrentIndex(index)
            elif isinstance(control, QLineEdit):
                control.setText(str(value))
            elif isinstance(control, QCheckBox):
                control.setChecked(bool(value))
            elif isinstance(control, QSpinBox):
                try:
                    control.setValue(int(value))
                except (TypeError, ValueError):
                    pass

    def _schedule_workspace_save(self, *_args):
        if hasattr(self, "autosave_timer"):
            self.autosave_timer.start()

    def _persist_workspace(self):
        for key, table in (("data/channels", self.table_channels), ("data/contacts", self.table_contacts)):
            rows = []
            for row_index in range(table.rowCount()):
                rows.append([
                    table.item(row_index, column).text() if table.item(row_index, column) else ""
                    for column in range(table.columnCount())
                ])
            self.workspace_data[key] = rows
        self.workspace_data["controls"] = {
            key: control.currentText() if isinstance(control, QComboBox)
            else control.text() if isinstance(control, QLineEdit)
            else control.isChecked() if isinstance(control, QCheckBox)
            else control.value()
            for key, control in self.persistent_controls
        }
        try:
            os.makedirs(self.workspace_dir, exist_ok=True)
            temporary_path = self.workspace_path + ".tmp"
            with open(temporary_path, "w", encoding="utf-8") as destination:
                json.dump(self.workspace_data, destination, ensure_ascii=False, separators=(",", ":"))
            os.replace(temporary_path, self.workspace_path)
        except OSError as exc:
            self.statusBar().showMessage(f"Çalışma verileri kaydedilemedi: {exc}", 10000)

    def closeEvent(self, event):
        active_threads = [thread for thread in (self.handshake_thread, self.flash_thread, self.analysis_thread) if thread and thread.isRunning()]
        if active_threads:
            reply = QMessageBox.question(
                self,
                "Devam eden işlem",
                "Bir bağlantı veya ön kontrol işlemi sürüyor. Tamamlanmasını bekleyerek çıkılsın mı?",
                QMessageBox.Yes | QMessageBox.No,
                QMessageBox.Yes,
            )
            if reply != QMessageBox.Yes:
                event.ignore()
                return
            for thread in active_threads:
                thread.wait()
        self.settings.setValue("window/geometry", self.saveGeometry())
        if hasattr(self, "autosave_timer"):
            self.autosave_timer.stop()
        self._persist_workspace()
        self.settings.sync()
        event.accept()

    def init_ui(self):
        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        root_layout = QVBoxLayout(central_widget)
        root_layout.setContentsMargins(0, 0, 0, 0)
        root_layout.setSpacing(0)

        # 1. ÜST HEADER
        header = QWidget()
        header.setObjectName("header")
        header_layout = QHBoxLayout(header)
        header_layout.setContentsMargins(18, 10, 18, 10)

        title_layout = QHBoxLayout()
        title_layout.setSpacing(10)
        title_lbl = QLabel("AT-D878 STUDIO")
        title_lbl.setObjectName("app_title")
        badge_lbl = QLabel("OPEN FIRMWARE")
        badge_lbl.setObjectName("app_badge")
        hw_lbl = QLabel("D878UV2A-BORD")
        hw_lbl.setObjectName("hw_badge")
        call_lbl = QLabel("TA5THK")
        call_lbl.setStyleSheet("background-color: #064E3B; color: #34D399; border: 1px solid #059669; font-weight: 700; font-size: 11px; padding: 2px 8px; border-radius: 4px;")
        
        title_layout.addWidget(title_lbl)
        title_layout.addWidget(badge_lbl)
        title_layout.addWidget(hw_lbl)
        title_layout.addWidget(call_lbl)
        header_layout.addLayout(title_layout)

        header_layout.addStretch()

        # Port ve Bağlantı Kontrolleri
        port_box = QHBoxLayout()
        port_box.setSpacing(8)
        self.conn_indicator = QLabel("● HAZIR")
        self.conn_indicator.setStyleSheet("color: #10B981; font-weight: 700; font-size: 11px;")
        port_box.addWidget(self.conn_indicator)

        self.port_combo = QComboBox()
        self.port_combo.setMinimumWidth(180)
        self.refresh_ports()
        port_box.addWidget(self.port_combo)

        btn_refresh = QPushButton("⟳")
        btn_refresh.setToolTip("Portları Yenile")
        btn_refresh.setFixedWidth(32)
        btn_refresh.clicked.connect(self.refresh_ports)
        port_box.addWidget(btn_refresh)

        self.btn_connect = QPushButton("🔗 Bağlan & Test Et")
        self.btn_connect.setProperty("class", "success")
        self.btn_connect.setToolTip("AnyTone Telsiz ile USB üzerinden el sıkış ve model doğrula")
        self.btn_connect.clicked.connect(self.test_device_handshake)
        port_box.addWidget(self.btn_connect)

        btn_import_rdt = QPushButton("RDT Dosyasını İncele")
        btn_import_rdt.setProperty("class", "primary")
        btn_import_rdt.clicked.connect(self.inspect_codeplug_rdt)
        port_box.addWidget(btn_import_rdt)

        header_layout.addLayout(port_box)
        root_layout.addWidget(header)

        # 2. ANA GÖVDE: SIDEBAR + CONTENT
        body = QWidget()
        body_layout = QHBoxLayout(body)
        body_layout.setContentsMargins(0, 0, 0, 0)
        body_layout.setSpacing(0)

        # Sidebar
        sidebar = QWidget()
        sidebar.setObjectName("sidebar")
        sb_layout = QVBoxLayout(sidebar)
        sb_layout.setContentsMargins(0, 10, 0, 10)
        sb_layout.setSpacing(2)

        self.btn_nav_dash = QPushButton("📊 Cihaz Paneli")
        self.btn_nav_chan = QPushButton("📻 Kanallar & VFO")
        self.btn_nav_cont = QPushButton("👥 DMR Kişiler")
        self.btn_nav_aprs = QPushButton("📡 APRS & GPS Dolaşım")
        self.btn_nav_keys = QPushButton("🎛️ Cihaz Ayarları & Tuşlar")
        self.btn_nav_sim  = QPushButton("📺 Telsiz LCD Simülatörü")
        self.btn_nav_boot = QPushButton("🎨 Açılış Ekranı (BMP)")
        self.btn_nav_flash= QPushButton("⚡ Çoklu Flasher")
        self.btn_nav_docs = QPushButton("📖 Donanım Belgeleri")

        self.nav_buttons = [
            self.btn_nav_dash, self.btn_nav_chan, self.btn_nav_cont,
            self.btn_nav_aprs, self.btn_nav_keys, self.btn_nav_sim,
            self.btn_nav_boot, self.btn_nav_flash, self.btn_nav_docs
        ]

        for i, btn in enumerate(self.nav_buttons):
            btn.setCheckable(True)
            btn.clicked.connect(lambda checked, idx=i: self.switch_page(idx))
            sb_layout.addWidget(btn)

        sb_layout.addStretch()
        
        # Sürüm ve Telif Bilgisi
        ver_lbl = QLabel(" AnyTone AT-D878UVII Plus\n Donanım: D878UV2A-BORD\n V4.00 Resmi Uyumlu")
        ver_lbl.setStyleSheet("color: #64748B; font-size: 11px; padding: 12px;")
        sb_layout.addWidget(ver_lbl)

        body_layout.addWidget(sidebar)

        # Sağ İçerik Alanı (Stacked Widget)
        self.pages = QStackedWidget()
        self.pages.setContentsMargins(18, 16, 18, 16)

        self.pages.addWidget(self.create_dashboard_page())    # 0
        self.pages.addWidget(self.create_channels_page())     # 1
        self.pages.addWidget(self.create_contacts_page())     # 2
        self.pages.addWidget(self.create_aprs_page())         # 3
        self.pages.addWidget(self.create_keys_page())         # 4
        self.pages.addWidget(self.create_simulator_page())    # 5
        self.pages.addWidget(self.create_boot_screen_page())  # 6
        self.pages.addWidget(self.create_flasher_page())      # 7
        self.pages.addWidget(self.create_docs_page())         # 8

        body_layout.addWidget(self.pages)
        root_layout.addWidget(body)

        self.statusBar().showMessage("Studio hazır. Kanal ve kişi tabloları bu bilgisayarda saklanır; telsiz bağlantısı ayrı doğrulanır.")
        self.switch_page(0)

    def switch_page(self, index):
        self.pages.setCurrentIndex(index)
        for i, btn in enumerate(self.nav_buttons):
            btn.setChecked(i == index)

    # -------------------------------------------------------------
    # SAYFA 0: DASHBOARD
    # -------------------------------------------------------------
    def create_dashboard_page(self):
        page = QWidget()
        layout = QVBoxLayout(page)
        layout.setSpacing(14)
        layout.addWidget(QLabel("Bu panel proje hedef profilini gösterir; canlı telsiz telemetrisi değildir."))

        cards_layout = QHBoxLayout()
        cards = [
            ("HEDEF PROFİL", "AT-D878UVII Plus", "#38BDF8"),
            ("KART REVİZYONU", "D878UV2A-BORD", "#818CF8"),
            ("ANA İŞLEMCİ", "GD32F303VGT6 (120MHz)", "#34D399"),
            ("ÇAĞRI KODU", "TA5THK", "#FBBF24")
        ]
        for title, val, col in cards:
            card = QFrame()
            card.setStyleSheet("background-color: #111827; border: 1px solid #1F2937; border-radius: 8px; padding: 14px;")
            c_lay = QVBoxLayout(card)
            t_lbl = QLabel(title)
            t_lbl.setStyleSheet("color: #94A3B8; font-size: 11px; font-weight: 700; letter-spacing: 0.5px;")
            v_lbl = QLabel(val)
            v_lbl.setStyleSheet(f"color: {col}; font-size: 15px; font-weight: 800; margin-top: 4px;")
            c_lay.addWidget(t_lbl)
            c_lay.addWidget(v_lbl)
            cards_layout.addWidget(card)
        layout.addLayout(cards_layout)

        # Bellek Haritası
        group_mem = QGroupBox("Proje bellek haritası (referans; cihazdan okunmadı)")
        m_lay = QVBoxLayout(group_mem)
        mem_items = [
            ("0x08000000 - 0x08003FFF (16 KB)", "Üretici boot alanı (proje varsayımı)", "#EF4444", "Bu uygulama yazma yapmaz"),
            ("0x08004000 - 0x080FFFFF (1008 KB)", "Uygulama alanı (adres profili)", "#10B981", "Harita cihazda doğrulanmadı"),
            ("0x20000000 - 0x20017FFF (96 KB)", "Dahili SRAM (Stack & Heap & DMA Framebuffer)", "#38BDF8", "RAM (Değişkenler & 40KB Ekran Tamponu)"),
            ("Harici flash", "İkon paketi / kullanıcı verileri", "#FBBF24", "Kapasite cihazda doğrulanmadı")
        ]
        for addr, name, col, desc in mem_items:
            row = QHBoxLayout()
            a_lbl = QLabel(f"<code>{addr}</code>")
            a_lbl.setStyleSheet(f"color: {col}; font-weight: 700; font-size: 12px; min-width: 250px;")
            n_lbl = QLabel(f"<b>{name}</b>")
            d_lbl = QLabel(desc)
            d_lbl.setStyleSheet("color: #94A3B8; font-size: 12px;")
            row.addWidget(a_lbl)
            row.addWidget(n_lbl)
            row.addStretch()
            row.addWidget(d_lbl)
            m_lay.addLayout(row)
        layout.addWidget(group_mem)

        # Donanım Katmanları
        group_specs = QGroupBox("Proje donanım notları (telsizden okunmadı)")
        specs_layout = QVBoxLayout(group_specs)
        specs = [
            "• <b>RF profili:</b> Auctus 1846N (proje notlarında belirtilen) [PB1, PB2]",
            "• <b>DMR baseband:</b> Sicomm/CML SCT3258TD (proje notlarında belirtilen) [PE0–PE7]",
            "• <b>Ekran profili:</b> 1.77 inç TFT, 160×128 (proje notlarında belirtilen)",
            "• <b>Alt kart:</b> LinkBoard BHT V2.00 (proje notlarında belirtilen)",
            "• <b>Ses profili:</b> TLV320AIC3204 (proje notlarında belirtilen)"
        ]
        for s in specs:
            lbl = QLabel(s)
            lbl.setStyleSheet("font-size: 12px; padding: 2px 0;")
            specs_layout.addWidget(lbl)
        layout.addWidget(group_specs)
        layout.addStretch()
        return page

    # -------------------------------------------------------------
    # SAYFA 1: KANALLAR VE VFO
    # -------------------------------------------------------------
    def create_channels_page(self):
        page = QWidget()
        layout = QVBoxLayout(page)

        top_bar = QHBoxLayout()
        self.chan_search = QLineEdit()
        self.chan_search.setPlaceholderText("🔍 Kanal adı, frekans veya mod ara...")
        self.chan_search.setMaximumWidth(260)
        self.chan_search.textChanged.connect(self.filter_channels)
        top_bar.addWidget(self.chan_search)
        top_bar.addStretch()

        btn_add = QPushButton("+ Yeni Kanal Ekle")
        btn_add.clicked.connect(self.add_channel_row)
        top_bar.addWidget(btn_add)

        btn_remove = QPushButton("Seçileni Sil")
        btn_remove.clicked.connect(lambda: self.delete_selected_rows(self.table_channels, "kanal"))
        top_bar.addWidget(btn_remove)

        btn_import = QPushButton("CSV İçe Aktar")
        btn_import.clicked.connect(self.import_channels_csv)
        top_bar.addWidget(btn_import)
        
        btn_export = QPushButton("CSV Dışa Aktar")
        btn_export.clicked.connect(self.export_channels_csv)
        top_bar.addWidget(btn_export)
        
        layout.addLayout(top_bar)
        layout.addWidget(QLabel("Yerel kanal çalışma listesi · CSV ile içe aktarın veya yeni kanal ekleyin · değişiklikler bu bilgisayarda saklanır · telsize yazma kapalı"))

        self.table_channels = QTableWidget(0, 8)
        self.table_channels.setHorizontalHeaderLabels([
            "No", "Kanal Adı", "RX Frekansı (MHz)", "TX Frekansı (MHz)", "Mod", "Güç Seviyesi", "Bant Genişliği", "Ton / Renk Kodu / TG"
        ])
        self.table_channels.horizontalHeader().setSectionResizeMode(QHeaderView.Stretch)
        self.table_channels.setSelectionBehavior(QAbstractItemView.SelectRows)
        self.table_channels.setSelectionMode(QAbstractItemView.ExtendedSelection)
        self.table_channels.setAlternatingRowColors(True)
        layout.addWidget(self.table_channels)
        return page

    def filter_channels(self, query):
        q = query.lower()
        for r in range(self.table_channels.rowCount()):
            match = False
            for c in range(self.table_channels.columnCount()):
                item = self.table_channels.item(r, c)
                if item and q in item.text().lower():
                    match = True
                    break
            self.table_channels.setRowHidden(r, not match)

    def add_channel_row(self):
        r = self.table_channels.rowCount()
        self.table_channels.insertRow(r)
        self.table_channels.setItem(r, 0, QTableWidgetItem(str(r + 1)))
        self.table_channels.setItem(r, 1, QTableWidgetItem("YENI KANAL"))
        self.table_channels.setItem(r, 2, QTableWidgetItem("145.00000"))
        self.table_channels.setItem(r, 3, QTableWidgetItem("145.00000"))
        self.table_channels.setItem(r, 4, QTableWidgetItem("FM"))
        self.table_channels.setItem(r, 5, QTableWidgetItem("Turbo"))
        self.table_channels.setItem(r, 6, QTableWidgetItem("25 kHz"))
        self.table_channels.setItem(r, 7, QTableWidgetItem("CSQ"))

    def delete_selected_rows(self, table, record_type):
        selected_rows = sorted({item.row() for item in table.selectedItems()}, reverse=True)
        if not selected_rows:
            QMessageBox.information(self, "Seçim Yok", f"Silmek için en az bir {record_type} satırı seçin.")
            return
        reply = QMessageBox.question(
            self,
            "Satırları Sil",
            f"Seçili {len(selected_rows)} {record_type} satırı silinsin mi?",
            QMessageBox.Yes | QMessageBox.No,
            QMessageBox.No,
        )
        if reply != QMessageBox.Yes:
            return
        for row in selected_rows:
            table.removeRow(row)
        self._schedule_workspace_save()

    def import_channels_csv(self):
        path, _ = QFileDialog.getOpenFileName(self, "Kanal CSV Dosyası Seç", self.project_root, "CSV Dosyası (*.csv)")
        if not path:
            return
        aliases = {
            "no": 0, "number": 0, "channelno": 0, "channelnumber": 0,
            "kanaladı": 1, "channelname": 1, "channel": 1, "kanal": 1, "name": 1,
            "rxfrekansımhz": 2, "rxfrekansı": 2, "rxfreq": 2, "rxfrequency": 2, "receivefrequency": 2,
            "txfrekansımhz": 3, "txfrekansı": 3, "txfreq": 3, "txfrequency": 3, "transmitfrequency": 3,
            "mod": 4, "mode": 4, "modulation": 4,
            "güçseviyesi": 5, "power": 5, "powerlevel": 5,
            "bantgenişliği": 6, "bandwidth": 6,
            "tonrenkkodutg": 7, "tone": 7, "colorcodetg": 7, "colorcode": 7,
        }

        def normalize_header(value):
            return "".join(ch for ch in value.casefold() if ch.isalnum())

        try:
            with open(path, "r", newline="", encoding="utf-8-sig") as stream:
                sample = stream.read(8192)
                stream.seek(0)
                try:
                    dialect = csv.Sniffer().sniff(sample, delimiters=",;\t")
                except csv.Error:
                    dialect = csv.excel
                rows = list(csv.reader(stream, dialect))
            if not rows:
                raise ValueError("Dosyada satır bulunamadı.")
            normalized = [normalize_header(value) for value in rows[0]]
            normalized_aliases = {normalize_header(key): value for key, value in aliases.items()}
            header_map = {normalized_aliases[value]: index for index, value in enumerate(normalized) if value in normalized_aliases}
            has_header = 2 in header_map and 3 in header_map and 1 in header_map
            data_rows = rows[1:] if has_header else rows
            if len(data_rows) > 10000:
                raise ValueError("Güvenlik sınırı aşıldı: tek seferde en fazla 10.000 kanal alınabilir.")

            imported = []
            for source_row, values in enumerate(data_rows, start=2 if has_header else 1):
                if not values or not any(value.strip() for value in values):
                    continue
                if has_header:
                    row = [values[header_map[index]].strip() if index in header_map and header_map[index] < len(values) else "" for index in range(8)]
                else:
                    row = [values[i].strip() if i < len(values) else "" for i in range(8)]
                row[0] = row[0] or str(len(imported) + 1)
                row[4] = row[4].upper() or "FM"
                if row[4] in ("ANALOG", "ANALOGUE"):
                    row[4] = "FM"
                elif row[4] == "DIGITAL":
                    row[4] = "DMR"
                try:
                    rx, tx = float(row[2].replace(",", ".")), float(row[3].replace(",", "."))
                except ValueError as exc:
                    raise ValueError(f"Satır {source_row}: RX/TX frekansı sayı olmalı.") from exc
                valid_band = lambda freq: math.isfinite(freq) and (134.0 <= freq <= 174.0 or 400.0 <= freq <= 520.0)
                if not valid_band(rx) or not valid_band(tx):
                    raise ValueError(f"Satır {source_row}: frekans 134–174 veya 400–520 MHz aralığında olmalı.")
                if not row[1] or len(row[1]) > 16:
                    raise ValueError(f"Satır {source_row}: kanal adı 1–16 karakter olmalı.")
                if row[4] not in ("FM", "DMR"):
                    raise ValueError(f"Satır {source_row}: mod FM veya DMR olmalı.")
                row[2], row[3] = f"{rx:.5f}", f"{tx:.5f}"
                imported.append(row)

            if not imported:
                raise ValueError("İçe aktarılabilir kanal bulunamadı.")
            self.table_channels.setRowCount(len(imported))
            for row_index, values in enumerate(imported):
                for column, value in enumerate(values):
                    self.table_channels.setItem(row_index, column, QTableWidgetItem(value))
            self.filter_channels(self.chan_search.text())
            QMessageBox.information(self, "CSV İçe Aktarıldı", f"{len(imported)} kanal doğrulandı ve tabloya yüklendi.\n\nBu işlem telsize yazma yapmadı.")
        except (OSError, csv.Error, UnicodeError, ValueError) as exc:
            QMessageBox.critical(self, "Kanal CSV Hatası", str(exc))

    def export_channels_csv(self):
        path, _ = QFileDialog.getSaveFileName(self, "Kanal Listesi CSV Kaydet", self.project_root, "CSV Dosyası (*.csv)")
        if not path:
            return
        try:
            with open(path, "w", newline="", encoding="utf-8-sig") as f:
                writer = csv.writer(f)
                headers = [self.table_channels.horizontalHeaderItem(c).text() for c in range(self.table_channels.columnCount())]
                writer.writerow(headers)
                for r in range(self.table_channels.rowCount()):
                    row = [self.table_channels.item(r, c).text() if self.table_channels.item(r, c) else "" for c in range(self.table_channels.columnCount())]
                    writer.writerow(row)
            QMessageBox.information(self, "Başarılı", f"Kanal listesi başarıyla aktarıldı:\n{path}")
        except Exception as e:
            QMessageBox.critical(self, "Hata", f"CSV yazma hatası: {e}")

    # -------------------------------------------------------------
    # SAYFA 2: DMR DİJİTAL KİŞİLER (RADIOID DATABASE)
    # -------------------------------------------------------------
    def create_contacts_page(self):
        page = QWidget()
        layout = QVBoxLayout(page)

        desc_lbl = QLabel(
            "<b>DMR kişi listesi önizlemesi:</b> JSON veya CSV dosyalarını içe aktarın, arayın ve bu bilgisayardaki Studio oturumunda saklayın. "
            "Bu ekran ağdan RadioID indirmez ve listeyi telsize yazmaz."
        )
        desc_lbl.setStyleSheet("color: #94A3B8; font-size: 12px; margin-bottom: 6px;")
        desc_lbl.setWordWrap(True)
        layout.addWidget(desc_lbl)

        top_bar = QHBoxLayout()
        self.contact_search = QLineEdit()
        self.contact_search.setPlaceholderText("🔍 DMR ID, Çağrı Kodu veya İsim ara...")
        self.contact_search.setMaximumWidth(240)
        self.contact_search.textChanged.connect(self.filter_contacts)
        top_bar.addWidget(self.contact_search)
        top_bar.addStretch()

        btn_add_contact = QPushButton("+ Kişi Ekle")
        btn_add_contact.clicked.connect(self.add_contact_row)
        top_bar.addWidget(btn_add_contact)

        btn_remove_contact = QPushButton("Seçileni Sil")
        btn_remove_contact.clicked.connect(lambda: self.delete_selected_rows(self.table_contacts, "kişi"))
        top_bar.addWidget(btn_remove_contact)

        btn_import_json = QPushButton("RadioID JSON İçe Aktar")
        btn_import_json.clicked.connect(self.import_radioid_json)
        top_bar.addWidget(btn_import_json)

        btn_import_csv = QPushButton("CSV İçe Aktar")
        btn_import_csv.clicked.connect(self.import_radioid_csv)
        top_bar.addWidget(btn_import_csv)

        btn_export_csv = QPushButton("CSV Dışa Aktar")
        btn_export_csv.clicked.connect(self.export_contacts_csv)
        top_bar.addWidget(btn_export_csv)

        layout.addLayout(top_bar)

        self.table_contacts = QTableWidget(0, 6)
        self.table_contacts.setHorizontalHeaderLabels([
            "DMR ID", "Çağrı Kodu", "İsim", "Şehir", "Ülke", "Tür / Açıklama"
        ])
        self.table_contacts.horizontalHeader().setSectionResizeMode(QHeaderView.Stretch)
        self.table_contacts.setSelectionBehavior(QAbstractItemView.SelectRows)
        self.table_contacts.setSelectionMode(QAbstractItemView.ExtendedSelection)
        self.table_contacts.setAlternatingRowColors(True)
        layout.addWidget(self.table_contacts)
        return page

    def load_sample_contacts(self):
        self.table_contacts.setRowCount(0)

    def filter_contacts(self, query):
        q = query.lower()
        for r in range(self.table_contacts.rowCount()):
            match = False
            for c in range(self.table_contacts.columnCount()):
                it = self.table_contacts.item(r, c)
                if it and q in it.text().lower():
                    match = True
                    break
            self.table_contacts.setRowHidden(r, not match)

    def add_contact_row(self):
        row = self.table_contacts.rowCount()
        self.table_contacts.insertRow(row)
        self.table_contacts.setItem(row, 0, QTableWidgetItem(""))
        for column in range(1, self.table_contacts.columnCount()):
            self.table_contacts.setItem(row, column, QTableWidgetItem(""))
        self.table_contacts.setCurrentCell(row, 0)
        self.table_contacts.editItem(self.table_contacts.item(row, 0))

    def export_contacts_csv(self):
        path, _ = QFileDialog.getSaveFileName(self, "Kişi Listesi CSV Kaydet", self.project_root, "CSV Dosyası (*.csv)")
        if not path:
            return
        try:
            with open(path, "w", newline="", encoding="utf-8-sig") as stream:
                writer = csv.writer(stream)
                writer.writerow([self.table_contacts.horizontalHeaderItem(column).text() for column in range(6)])
                for row in range(self.table_contacts.rowCount()):
                    writer.writerow([
                        self.table_contacts.item(row, column).text() if self.table_contacts.item(row, column) else ""
                        for column in range(6)
                    ])
            QMessageBox.information(self, "CSV Dışa Aktarıldı", f"{self.table_contacts.rowCount()} kişi satırı kaydedildi.\n{path}")
        except OSError as exc:
            QMessageBox.critical(self, "CSV Dışa Aktarma Hatası", str(exc))

    def import_radioid_json(self):
        path, _ = QFileDialog.getOpenFileName(self, "RadioID JSON Seç", self.project_root, "JSON Dosyaları (*.json)")
        if not path:
            return
        try:
            with open(path, "r", encoding="utf-8") as f:
                data = json.load(f)
            if isinstance(data, list):
                users = data
            elif isinstance(data, dict):
                users = data.get("users", data.get("contacts", data.get("results", [])))
            else:
                raise ValueError("JSON kökü bir kişi listesi veya users/contacts/results alanı olmalı.")
            if not isinstance(users, list):
                raise ValueError("JSON içindeki kişi alanı liste biçiminde değil.")
            records = [self._contact_to_row(user) for user in users if isinstance(user, dict)]
            self._load_contact_rows(records)
        except (OSError, UnicodeError, json.JSONDecodeError, ValueError) as exc:
            QMessageBox.critical(self, "JSON İçe Aktarma Hatası", str(exc))

    @staticmethod
    def _contact_to_row(record):
        def normalize(value):
            return "".join(character for character in str(value).casefold() if character.isalnum())

        values = {normalize(key): value for key, value in record.items()}

        def field(*keys):
            for key in keys:
                value = values.get(normalize(key))
                if value not in (None, ""):
                    return str(value).strip()
            return ""

        name = field("name", "fullname", "displayname")
        if not name:
            name = " ".join(part for part in (field("firstname", "fname", "first"), field("lastname", "surname", "lname", "last")) if part)
        return [
            field("id", "radioid", "dmrid", "radio_id"),
            field("callsign", "call", "callsign"),
            name,
            field("city", "town"),
            field("country"),
            field("remarks", "type", "description"),
        ]

    def _load_contact_rows(self, records):
        visible = []
        record_count = 0
        for row in records:
            if not any(value for value in row):
                continue
            record_count += 1
            if len(visible) < 1000:
                visible.append(row)
        if not record_count:
            raise ValueError("İçe aktarılabilir kişi kaydı bulunamadı.")
        self.table_contacts.setRowCount(len(visible))
        for row_index, values in enumerate(visible):
            for column, value in enumerate(values):
                self.table_contacts.setItem(row_index, column, QTableWidgetItem(value))
        self.filter_contacts(self.contact_search.text())
        shown = len(visible)
        suffix = f" İlk {shown} kayıt gösteriliyor." if record_count > shown else ""
        QMessageBox.information(
            self,
            "Kişi Listesi Yüklendi",
            f"{record_count} kayıt ayrıştırıldı; {shown} kayıt Studio tablosuna yüklendi.{suffix}\n\nTelsize aktarım yapılmadı."
        )

    def import_radioid_csv(self):
        path, _ = QFileDialog.getOpenFileName(self, "RadioID CSV Seç", self.project_root, "CSV Dosyaları (*.csv)")
        if not path:
            return
        try:
            def normalize(value):
                return "".join(character for character in value.casefold() if character.isalnum())

            field_names = {
                "radioid", "id", "dmrid", "callsign", "firstname", "fname", "lastname", "surname",
                "name", "city", "country", "remarks", "type", "description",
            }
            def read_csv(encoding):
                with open(path, "r", newline="", encoding=encoding) as stream:
                    sample = stream.read(8192)
                    stream.seek(0)
                    try:
                        dialect = csv.Sniffer().sniff(sample, delimiters=",;\t")
                    except csv.Error:
                        dialect = csv.excel
                    reader = csv.reader(stream, dialect)
                    first = next(reader, None)
                    if first is None:
                        raise ValueError("CSV dosyası boş.")
                    names = [normalize(value) for value in first]
                    has_header = any(value in field_names for value in names)
                    data_rows = reader if has_header else itertools.chain((first,), reader)
                    header_indexes = {name: index for index, name in enumerate(names)}

                    def row_records():
                        for values in data_rows:
                            if has_header:
                                record = {
                                    name: values[index] if index < len(values) else ""
                                    for name, index in header_indexes.items()
                                }
                            else:
                                record = {
                                    "id": values[0] if len(values) > 0 else "",
                                    "callsign": values[1] if len(values) > 1 else "",
                                    "name": values[2] if len(values) > 2 else "",
                                    "city": values[3] if len(values) > 3 else "",
                                    "country": values[4] if len(values) > 4 else "",
                                    "remarks": values[5] if len(values) > 5 else "",
                                }
                            yield self._contact_to_row(record)

                    self._load_contact_rows(row_records())

            try:
                read_csv("utf-8-sig")
            except UnicodeDecodeError:
                read_csv("cp1254")
        except (OSError, UnicodeError, csv.Error, ValueError) as exc:
            QMessageBox.critical(self, "CSV İçe Aktarma Hatası", str(exc))

    # -------------------------------------------------------------
    # SAYFA 3: APRS VE GPS DOLAŞIM (ROAMING)
    # -------------------------------------------------------------
    def create_aprs_page(self):
        page = QWidget()
        layout = QVBoxLayout(page)
        layout.addWidget(QLabel("APRS alanları bu bilgisayardaki ayar taslağıdır; RF ve LinkBoard bağlantısı etkin değildir."))

        # Üst İki Sütunlu Kartlar
        h_split = QHBoxLayout()

        # Analog APRS Ayarları
        group_analog = QGroupBox("Analog APRS Ayar Taslağı (1200 Baud AFSK)")
        a_lay = QVBoxLayout(group_analog)

        row_freq = QHBoxLayout()
        row_freq.addWidget(QLabel("APRS Frekansı (MHz):"))
        self.combo_aprs_freq = QComboBox()
        self.combo_aprs_freq.addItems(["144.8000 (Avrupa / Türkiye)", "144.3900 (Kuzey Amerika)", "145.1750 (Avustralya)"])
        row_freq.addWidget(self.combo_aprs_freq)
        a_lay.addLayout(row_freq)

        row_call = QHBoxLayout()
        row_call.addWidget(QLabel("Çağrı Kodu & SSID:"))
        self.txt_aprs_call = QLineEdit("TA5THK-7")
        row_call.addWidget(self.txt_aprs_call)
        a_lay.addLayout(row_call)

        row_path = QHBoxLayout()
        row_path.addWidget(QLabel("APRS Yolu (Path):"))
        self.txt_aprs_path = QLineEdit("WIDE1-1, WIDE2-1")
        row_path.addWidget(self.txt_aprs_path)
        a_lay.addLayout(row_path)

        row_interval = QHBoxLayout()
        row_interval.addWidget(QLabel("İletim Aralığı (Beacon):"))
        self.combo_aprs_interval = QComboBox()
        self.combo_aprs_interval.addItems(["Akıllı Gönderim (SmartBeaconing)", "Manuel (PF3 Tuşu ile)", "1 Dakika", "3 Dakika", "5 Dakika", "10 Dakika"])
        row_interval.addWidget(self.combo_aprs_interval)
        a_lay.addLayout(row_interval)

        row_sym = QHBoxLayout()
        row_sym.addWidget(QLabel("APRS Sembolü:"))
        self.combo_aprs_sym = QComboBox()
        self.combo_aprs_sym.addItems(["/[ El Telsizi (Jogger/HT)", "/> Otomobil (Car)", "/- Ev İstasyonu (House)", "/k Kamyonet / SUV"])
        row_sym.addWidget(self.combo_aprs_sym)
        a_lay.addLayout(row_sym)

        row_comment = QHBoxLayout()
        row_comment.addWidget(QLabel("Durum Metni (Comment):"))
        self.txt_aprs_comment = QLineEdit("AnyTone AT-D878 Open Firmware V4.00")
        row_comment.addWidget(self.txt_aprs_comment)
        a_lay.addLayout(row_comment)

        h_split.addWidget(group_analog)

        # Dijital APRS & GPS Dolaşım
        group_dig = QGroupBox("DMR Dijital APRS & GPS Dolaşım (Roaming)")
        d_lay = QVBoxLayout(group_dig)

        row_dmr_aprs = QHBoxLayout()
        row_dmr_aprs.addWidget(QLabel("DMR Veri Zaman Dilimi:"))
        self.combo_dmr_ts = QComboBox()
        self.combo_dmr_ts.addItems(["Slot 1 (TS1)", "Slot 2 (TS2)"])
        row_dmr_aprs.addWidget(self.combo_dmr_ts)
        d_lay.addLayout(row_dmr_aprs)

        row_dmr_tg = QHBoxLayout()
        row_dmr_tg.addWidget(QLabel("APRS Ağ Geçidi TG:"))
        self.txt_dmr_aprs_tg = QLineEdit("286999 (BrandMeister TR)")
        row_dmr_tg.addWidget(self.txt_dmr_aprs_tg)
        d_lay.addLayout(row_dmr_tg)

        d_lay.addWidget(QLabel("<b>GPS Dolaşım (Roaming) Parametreleri:</b>"))
        self.chk_roam_auto = QCheckBox("Otomatik Röle Geçişi (GPS Roaming) Aktif")
        self.chk_roam_auto.setChecked(True)
        d_lay.addWidget(self.chk_roam_auto)

        row_roam_dist = QHBoxLayout()
        row_roam_dist.addWidget(QLabel("Maksimum Röle Mesafesi:"))
        self.spin_roam_dist = QSpinBox()
        self.spin_roam_dist.setRange(5, 200)
        self.spin_roam_dist.setValue(35)
        self.spin_roam_dist.setSuffix(" km")
        row_roam_dist.addWidget(self.spin_roam_dist)
        d_lay.addLayout(row_roam_dist)

        d_lay.addWidget(QLabel("LinkBoard durumu: <b>Bu uygulamada bağlı değil</b>"))
        d_lay.addStretch()

        h_split.addWidget(group_dig)
        layout.addLayout(h_split)

        # No verified LinkBoard APRS receive protocol is implemented in this app.
        layout.addWidget(QLabel("<b>APRS alıcı durumu:</b>"))
        self.aprs_log = QTextEdit()
        self.aprs_log.setReadOnly(True)
        self.aprs_log.setMaximumHeight(150)
        self.aprs_log.setPlainText("Canlı RF/LinkBoard alımı bağlı değil. Bu Studio ekranı APRS alımını veya yayınını izlemiyor.")
        layout.addWidget(self.aprs_log)

        return page

    # -------------------------------------------------------------
    # SAYFA 4: TUŞ PROGRAMLAMA VE CİHAZ AYARLARI
    # -------------------------------------------------------------
    def create_keys_page(self):
        page = QWidget()
        layout = QVBoxLayout(page)

        h_split = QHBoxLayout()

        # Tuş Atamaları
        layout.addWidget(QLabel("Tuş ve cihaz ayarları yerel taslak olarak düzenlenir; bu ekrandan telsize yazılmaz."))
        group_keys = QGroupBox("Programlanabilir Fonksiyon Tuşu Taslağı")
        k_lay = QVBoxLayout(group_keys)

        key_options = [
            "Monitör (Susturucu Aç)", "Güç Seviyesi Değiştir (T/H/M/L)", "VFO / Kanal Hafızası Geçişi",
            "Ters Frekans (Reverse)", "FM Radyo Aç/Kapa", "APRS Manuel Beacon Gönder",
            "Dim Shut (Ekran & LED Kapat)", "Tarama Başlat/Durdur", "Ses Kaydı Başlat", "Bölge Değiştir (Zone +/-)"
        ]
        self.key_assignment_combos = []

        def make_row(lbl_text, default_idx):
            r = QHBoxLayout()
            r.addWidget(QLabel(lbl_text))
            cb = QComboBox()
            cb.addItems(key_options)
            cb.setCurrentIndex(default_idx)
            self.key_assignment_combos.append(cb)
            r.addWidget(cb)
            return r

        k_lay.addLayout(make_row("PF1 Üst Yan Tuş (Kısa Basma):", 0))
        k_lay.addLayout(make_row("PF1 Üst Yan Tuş (Uzun Basma):", 1))
        k_lay.addLayout(make_row("PF2 Alt Yan Tuş (Kısa Basma):", 2))
        k_lay.addLayout(make_row("PF2 Alt Yan Tuş (Uzun Basma):", 4))
        k_lay.addLayout(make_row("PF3 Yeşil Tepe Tuşu (Kısa Basma):", 5))
        k_lay.addLayout(make_row("PF3 Yeşil Tepe Tuşu (Uzun Basma):", 6))
        h_split.addWidget(group_keys)

        # Ses ve Ekran Seçenekleri
        group_opt = QGroupBox("Ses Kazançları ve Ekran Davranışı")
        o_lay = QVBoxLayout(group_opt)

        r_mic_a = QHBoxLayout()
        r_mic_a.addWidget(QLabel("Analog Mikrofon Kazancı:"))
        self.spin_mic_a = QSpinBox()
        self.spin_mic_a.setRange(1, 5)
        self.spin_mic_a.setValue(3)
        self.spin_mic_a.setSuffix(" Kademe")
        r_mic_a.addWidget(self.spin_mic_a)
        o_lay.addLayout(r_mic_a)

        r_mic_d = QHBoxLayout()
        r_mic_d.addWidget(QLabel("DMR Dijital Mikrofon Kazancı:"))
        self.spin_mic_d = QSpinBox()
        self.spin_mic_d.setRange(1, 5)
        self.spin_mic_d.setValue(3)
        self.spin_mic_d.setSuffix(" Kademe")
        r_mic_d.addWidget(self.spin_mic_d)
        o_lay.addLayout(r_mic_d)

        r_light = QHBoxLayout()
        r_light.addWidget(QLabel("Ekran Işığı Süresi:"))
        self.combo_light = QComboBox()
        self.combo_light.addItems(["5 Saniye", "10 Saniye", "30 Saniye", "Sürekli Açık (Always On)"])
        self.combo_light.setCurrentIndex(1)
        r_light.addWidget(self.combo_light)
        o_lay.addLayout(r_light)

        self.chk_dim_shut = QCheckBox("Dim Shut Modu (Tuş basıldığında LED ve ekranı gizle)")
        o_lay.addWidget(self.chk_dim_shut)

        r_boot_text = QHBoxLayout()
        r_boot_text.addWidget(QLabel("Açılış Karşılama Metni:"))
        self.txt_boot_msg = QLineEdit("TA5THK FATIH")
        r_boot_text.addWidget(self.txt_boot_msg)
        o_lay.addLayout(r_boot_text)

        o_lay.addStretch()
        h_split.addWidget(group_opt)

        layout.addLayout(h_split)
        layout.addStretch()
        return page

    # -------------------------------------------------------------
    # -------------------------------------------------------------
    # SAYFA 5: CANLI TELSİZ LCD SİMÜLATÖRÜ & FIRMWARE EMÜLATÖRÜ
    # -------------------------------------------------------------
    def create_simulator_page(self):
        page = QWidget()
        layout = QHBoxLayout(page)
        layout.setSpacing(16)

        # Sol Kontrol ve Firmware Yükleme Paneli
        ctrl_frame = QFrame()
        ctrl_frame.setStyleSheet("background-color: #111827; border: 1px solid #1F2937; border-radius: 8px; padding: 14px;")
        c_lay = QVBoxLayout(ctrl_frame)

        # 1. Firmware Seçici
        c_lay.addWidget(QLabel("<b>1. Simüle Edilecek Firmware Dosyası (.CDD / .bin):</b>"))
        default_fw = os.path.join(self.project_root, "output_firmware", "D878UVII_V4.00_OPEN.CDD")
        self.sim_fw_path = default_fw if os.path.exists(default_fw) else ""
        self.sim_fw_label = QLabel(self.sim_fw_path if self.sim_fw_path else "Dosya seçilmedi")
        self.sim_fw_label.setStyleSheet("color: #38BDF8; font-size: 11px; font-weight: 600;")
        self.sim_fw_label.setWordWrap(True)
        c_lay.addWidget(self.sim_fw_label)

        btn_row_fw = QHBoxLayout()
        btn_sel_open = QPushButton("Açık Kaynak FW")
        btn_sel_open.clicked.connect(self.sim_select_open_fw)
        btn_sel_stock = QPushButton("Resmi V4.00 FW")
        btn_sel_stock.clicked.connect(self.sim_select_stock_fw)
        btn_sel_custom = QPushButton("Özel Seç...")
        btn_sel_custom.clicked.connect(self.sim_select_custom_fw)
        btn_row_fw.addWidget(btn_sel_open)
        btn_row_fw.addWidget(btn_sel_stock)
        btn_row_fw.addWidget(btn_sel_custom)
        c_lay.addLayout(btn_row_fw)

        # 2. Analiz & Emülasyon Başlatma Butonu
        self.btn_run_emulation = QPushButton("🔬 Firmware'i Analiz Et")
        self.btn_run_emulation.setProperty("class", "primary")
        self.btn_run_emulation.setMinimumHeight(42)
        self.btn_run_emulation.setStyleSheet("font-size: 13px; font-weight: 700;")
        self.btn_run_emulation.clicked.connect(self.run_firmware_analysis)
        c_lay.addWidget(self.btn_run_emulation)

        # Emülasyon ve Analiz Konsolu
        c_lay.addWidget(QLabel("<b>Statik kontrol ve kısmi CPU emülasyon raporu:</b>"))
        self.sim_report_text = QTextEdit()
        self.sim_report_text.setReadOnly(True)
        self.sim_report_text.setMinimumHeight(240)
        self.sim_report_text.setStyleSheet("background-color: #05080E; border: 1px solid #1E293B; color: #10B981; font-family: 'Consolas', monospace; font-size: 11px;")
        self.sim_report_text.append("[*] Yerel ön panel maketi hazır; tuşlar yalnızca ekrandaki arayüz durumunu değiştirir.")
        self.sim_report_text.append("[*] Firmware analizi telsize bağlanmaz. Periferikler ve LCD firmware davranışı tam emüle edilmez.")
        c_lay.addWidget(self.sim_report_text)

        desc = QLabel(
            "<b>Etkileşimli ön panel maketi:</b><br>"
            "• <b>[-] MENU:</b> Ana menüyü açar veya seçilen ayara girer.<br>"
            "• <b>[▲] [▼] D-Pad:</b> Menüde gezinir veya VFO frekansını 12.5 kHz adımlar.<br>"
            "• <b>[--] EXIT:</b> Bir üst menüye döner veya VFO ekranına çıkar.<br>"
            "• <b>[P1] / [P2]:</b> VFO A/B geçişi ve TX güç kademesi değişimi.<br>"
            "• <b>[PTT]:</b> Basılı tutarak TX mandala geçiş testi yapar.<br>"
            "• <b>Keypad (0-9):</b> Menü öğelerini doğrudan numaralayarak seçer veya frekans girer."
        )
        desc.setStyleSheet("color: #94A3B8; font-size: 11px; margin-top: 8px; line-height: 1.4;")
        desc.setWordWrap(True)
        c_lay.addWidget(desc)
        c_lay.addStretch()
        layout.addWidget(ctrl_frame, 1)

        # Sağ: Telsiz Kasası (AnyTone Handset Widget - Birebir Donanım Şablonu)
        right_frame = QFrame()
        right_frame.setStyleSheet("background-color: #0A0F1D; border: 1px solid #1E293B; border-radius: 8px; padding: 10px;")
        r_lay = QVBoxLayout(right_frame)
        r_lay.setContentsMargins(8, 8, 8, 8)

        r_lay.addWidget(QLabel("<b>AT-D878 tarzı yerel ön panel maketi:</b>"), alignment=Qt.AlignCenter)
        self.lcd_screen = RadioLCDWidget(scale=1.5)
        self.handset = AnyToneHandsetWidget(self.lcd_screen)
        r_lay.addWidget(self.handset, alignment=Qt.AlignCenter)

        layout.addWidget(right_frame, 0)
        return page

    def sim_select_open_fw(self):
        p = os.path.join(self.project_root, "output_firmware", "D878UVII_V4.00_OPEN.CDD")
        if not os.path.exists(p):
            p = os.path.join(self.project_root, "firmware", "build", "at_d878_fw.bin")
        if os.path.exists(p):
            self.sim_fw_path = p
            self.sim_fw_label.setText(p)
        else:
            QMessageBox.information(self, "Açık Firmware Bulunamadı", "Derlenmiş firmware paketi bulunamadı. Önce bir paket oluşturun veya Özel Seç ile dosya açın.")

    def sim_select_stock_fw(self):
        p = os.path.join(self.project_root, "tools", "firmware_analyzer", "D878UVII_V4.00 FW", "D878UVII_V4.00_20250725.CDD")
        if os.path.exists(p):
            self.sim_fw_path = p
            self.sim_fw_label.setText(p)
        else:
            QMessageBox.information(self, "Stok Firmware Bulunamadı", "Stok firmware bu çalışma klasöründe yok. Dosyayı Özel Seç ile açabilirsiniz.")

    def sim_select_custom_fw(self):
        path, _ = QFileDialog.getOpenFileName(self, "Simülasyon İçin Firmware Seç", self.project_root, "Firmware Binary (*.CDD *.bin)")
        if path:
            self.sim_fw_path = path
            self.sim_fw_label.setText(path)

    def run_firmware_analysis(self):
        if self.analysis_thread and self.analysis_thread.isRunning():
            self.statusBar().showMessage("Firmware analizi zaten sürüyor.", 4000)
            return
        if not self.sim_fw_path or not os.path.exists(self.sim_fw_path):
            QMessageBox.critical(self, "Hata", "Lütfen geçerli bir firmware dosyası seçin!")
            return

        self.sim_report_text.clear()
        self.sim_report_text.append(f"[*] Firmware analiz ediliyor: {os.path.basename(self.sim_fw_path)}")
        self.btn_run_emulation.setEnabled(False)
        self.analysis_thread = FirmwareAnalysisThread(self.sim_fw_path, max_cycles=5000)
        self.analysis_thread.result_signal.connect(self.on_firmware_analysis_finished)
        self.analysis_thread.start()

    def on_firmware_analysis_finished(self, rep):
        self.btn_run_emulation.setEnabled(True)

        # Konsola Rapor Yaz
        self.sim_report_text.append(f"-> Dosya Boyutu: {rep.filesize} Byte ({round(rep.filesize/1024, 2)} KB)")
        self.sim_report_text.append(f"-> Initial SP: 0x{rep.initial_sp:08X} | Reset Handler: 0x{rep.reset_handler:08X}")
        self.sim_report_text.append(f"-> Vektör tablosu: {rep.vector_check_msg}")

        if rep.vectors_valid:
            self.sim_report_text.append(f"-> Kısmi CPU emülasyonu: {rep.instructions_executed} komut; durum: {rep.emulation_status}.")
            if rep.emulation_error:
                self.sim_report_text.append(f"-> Emülasyon notu: {rep.emulation_error}")
            if rep.registers:
                reg_str = " | ".join(f"{key}={value}" for key, value in rep.registers.items())
                self.sim_report_text.append(f"-> CPU kayıtçıları: {reg_str}")
            self.sim_report_text.append(f"-> Metin taraması model eşleşmesi: {rep.detected_model}")
            self.sim_report_text.append(f"-> Metin taraması çağrı kodu eşleşmesi: {rep.detected_callsign}")
            self.sim_report_text.append("-> Not: Bu sonuç güvenlik sertifikası değildir; MMIO/periferik ve gerçek LCD davranışı modellenmiyor.")
            self.statusBar().showMessage("Firmware vektör kontrolleri tamamlandı; emülasyon yalnızca kısmi CPU sandbox'ıdır.", 8000)
            QMessageBox.information(
                self, "Temel Firmware Kontrolleri Tamamlandı",
                f"• Dosya: {os.path.basename(rep.filepath)}\n"
                f"• Boyut: {rep.filesize} byte\n"
                f"• Vektör kontrolleri: geçti\n"
                f"• CPU emülasyon komut sayısı: {rep.instructions_executed}\n"
                f"• Emülasyon durumu: {rep.emulation_status}\n\n"
                "Bu analiz firmware güvenliğini veya telsizle uyumluluğunu kanıtlamaz; cihazda hiçbir işlem yapılmadı."
            )
        else:
            self.sim_report_text.append(f"[!] Vektör kontrolü başarısız: {rep.vector_check_msg}")
            self.statusBar().showMessage("Firmware vektör tablosu doğrulanamadı.", 8000)
            QMessageBox.warning(self, "Firmware Ön Kontrolü Başarısız", rep.vector_check_msg)

    def sim_ptt_down(self):
        self.lcd_screen.is_tx = True
        self.lcd_screen.update()

    def sim_ptt_up(self):
        self.lcd_screen.is_tx = False
        self.lcd_screen.update()

    def sim_switch_vfo(self, vfo):
        self.lcd_screen.active_vfo = vfo
        self.lcd_screen.update()

    def sim_toggle_power(self):
        p_seq = ["T", "H", "M", "L"]
        cur = self.lcd_screen.power_level
        nxt = p_seq[(p_seq.index(cur) + 1) % len(p_seq)]
        self.lcd_screen.power_level = nxt
        self.lcd_screen.update()

    def sim_send_aprs(self):
        self.statusBar().showMessage("APRS beacon önizlemesi seçildi; telsiz/LinkBoard bağlantısı olmadığı için RF yayını yapılmadı.", 6000)

    # -------------------------------------------------------------
    # SAYFA 6: AÇILIŞ EKRANI (BOOT SCREEN)
    # -------------------------------------------------------------
    def create_boot_screen_page(self):
        page = QWidget()
        layout = QHBoxLayout(page)

        left = QVBoxLayout()
        left.addWidget(QLabel("<b>Ön Tanımlı AnyTone BMP Temaları (160x128 16-bit):</b>"))
        self.screens_combo = QComboBox()
        self.screens_dir = os.path.join(self.release_dir, "D878UVscreens")
        if os.path.exists(self.screens_dir):
            bmps = [f for f in os.listdir(self.screens_dir) if f.endswith(".bmp")]
            self.screens_combo.addItems(bmps)
            self.screens_combo.currentTextChanged.connect(self.on_screen_selected)
        left.addWidget(self.screens_combo)

        btn_custom = QPushButton("Özel BMP Resmi Seç...")
        btn_custom.clicked.connect(self.select_custom_bmp)
        left.addWidget(btn_custom)

        desc = QLabel(
            "<b>Önizleme için BMP profili:</b><br>"
            "• Çözünürlük: 160 × 128 piksel<br>"
            "• Renk: 8-bit paletli, sıkıştırmasız BMP<br>"
            "• Dosya boyutu: 21.558 byte<br><br>"
            "<i>Studio dosyayı doğrular ve önizler. Telsize gönderim bu sürümde desteklenmiyor.</i>"
        )
        desc.setStyleSheet("color: #94A3B8; font-size: 12px; margin-top: 10px;")
        left.addWidget(desc)
        left.addStretch()
        layout.addLayout(left, 1)

        right = QVBoxLayout()
        right.addWidget(QLabel("<b>Telsiz Ekran Önizlemesi:</b>"))
        
        bezel = QFrame()
        bezel.setStyleSheet("background-color: #05080E; border: 3px solid #334155; border-radius: 12px; padding: 12px;")
        b_lay = QVBoxLayout(bezel)
        self.bmp_preview = QLabel()
        self.bmp_preview.setFixedSize(160 * 2, 128 * 2)
        self.bmp_preview.setStyleSheet("border: 1px solid #1E293B; background-color: black;")
        self.bmp_preview.setScaledContents(True)
        b_lay.addWidget(self.bmp_preview, alignment=Qt.AlignCenter)
        
        right.addWidget(bezel, alignment=Qt.AlignCenter)
        right.addStretch()
        layout.addLayout(right, 1)

        if self.screens_combo.count() > 0:
            self.on_screen_selected(self.screens_combo.currentText())
        return page

    # -------------------------------------------------------------
    # SAYFA 7: ÇOKLU FLASHER
    # -------------------------------------------------------------
    def create_flasher_page(self):
        page = QWidget()
        layout = QVBoxLayout(page)

        group_mode = QGroupBox("1. Güncelleme Modu (İşlemci / Kart Seçimi)")
        m_lay = QHBoxLayout(group_mode)
        self.rb_mcu = QRadioButton("Ana MCU Firmware (GD32)")
        self.rb_mcu.setChecked(True)
        self.rb_icon = QRadioButton("İkon / Font Flash (3.7 MB)")
        self.rb_sct = QRadioButton("DMR Baseband (SCT3258)")
        self.rb_link = QRadioButton("BT & APRS Kartı (LinkBoard)")
        m_lay.addWidget(self.rb_mcu)
        m_lay.addWidget(self.rb_icon)
        m_lay.addWidget(self.rb_sct)
        m_lay.addWidget(self.rb_link)
        layout.addWidget(group_mode)

        self.info_box = QLabel()
        self.info_box.setStyleSheet("background-color: #1E293B; border-left: 4px solid #38BDF8; color: #E2E8F0; padding: 10px; border-radius: 4px; font-size: 13px;")
        self.update_boot_instruction()
        self.rb_mcu.toggled.connect(self.update_boot_instruction)
        self.rb_icon.toggled.connect(self.update_boot_instruction)
        self.rb_sct.toggled.connect(self.update_boot_instruction)
        self.rb_link.toggled.connect(self.update_boot_instruction)
        layout.addWidget(self.info_box)

        group_pkg = QGroupBox("2. Firmware Paket Dosyası (.CDD / .bin)")
        p_lay = QVBoxLayout(group_pkg)

        file_h = QHBoxLayout()
        self.fw_file_label = QLabel()
        default_fw = os.path.join(self.project_root, "output_firmware", "D878UVII_V4.00_OPEN_ALPHA.CDD")
        self.fw_file_path = default_fw if os.path.exists(default_fw) else ""
        self.fw_file_label.setText(self.fw_file_path if self.fw_file_path else "Dosya seçilmedi")
        self.fw_file_label.setStyleSheet("color: #38BDF8; font-weight: 600;")
        file_h.addWidget(self.fw_file_label)

        btn_sel_custom = QPushButton("Açık Kaynak Paketi (.CDD)")
        btn_sel_custom.clicked.connect(self.select_fw_file)
        file_h.addWidget(btn_sel_custom)

        btn_sel_stock = QPushButton("Resmi V4.00 Stok (.CDD)")
        btn_sel_stock.clicked.connect(self.select_stock_fw)
        file_h.addWidget(btn_sel_stock)

        btn_sel_icon = QPushButton("Resmi İkon Paketi (3.7MB)")
        btn_sel_icon.clicked.connect(self.select_icon_pkg)
        file_h.addWidget(btn_sel_icon)

        btn_sel_link = QPushButton("LinkBoard BHT (.CDD)")
        btn_sel_link.clicked.connect(self.select_linkboard_pkg)
        file_h.addWidget(btn_sel_link)
        p_lay.addLayout(file_h)
        layout.addWidget(group_pkg)

        act_bar = QHBoxLayout()
        self.chk_dry_run = QCheckBox("Güvenli ön kontrol (dry-run) — cihaza yazma kapalı")
        self.chk_dry_run.setChecked(True)
        self.chk_dry_run.setEnabled(False)
        self.chk_dry_run.setToolTip("Gerçek yazma protokolü doğrulanmadığı için bu sürüm yalnızca dosya ön kontrolü yapar.")
        act_bar.addWidget(self.chk_dry_run)
        act_bar.addStretch()

        self.btn_ping_flasher = QPushButton("🔍 Bağlantı & Model Testi (Ping)")
        self.btn_ping_flasher.clicked.connect(self.test_device_handshake)
        act_bar.addWidget(self.btn_ping_flasher)

        self.btn_flash = QPushButton("Yüklemeyi Başlat (Start Flash)")
        self.btn_flash.setProperty("class", "success")
        self.btn_flash.setMinimumHeight(36)
        self.btn_flash.clicked.connect(self.start_flash)
        act_bar.addWidget(self.btn_flash)
        layout.addLayout(act_bar)

        self.progress_bar = QProgressBar()
        layout.addWidget(self.progress_bar)

        layout.addWidget(QLabel("<b>Canlı İletişim Konsolu:</b>"))
        self.log_text = QTextEdit()
        self.log_text.setReadOnly(True)
        self.log_text.setMaximumHeight(130)
        layout.addWidget(self.log_text)

        return page

    # -------------------------------------------------------------
    # SAYFA 8: BELGELER (DOCS)
    # -------------------------------------------------------------
    def create_docs_page(self):
        page = QWidget()
        layout = QVBoxLayout(page)
        layout.addWidget(QLabel("<b>Proje Teknik Raporları ve Dokümantasyon:</b>"))

        docs_list = [
            ("HARDWARE_ANALYSIS.md", "Donanım analiz raporu, GD32F303, AT1846S, SCT3258 detayları"),
            ("FEASIBILITY_REPORT.md", "Teknik fizibilite ve risk matrisi analizi"),
            ("PINMAP_D878UV2A.md", "Orijinal firmware'den deşifre edilen kesin GPIO pinout tablosu"),
            ("ANYTONE_BOOT_MODES.md", "4 farklı önyükleme (boot) modu ve tuş kombinasyonları kılavuzu"),
            ("RECOVERY_GUIDE.md", "Güvenli kurtarma ve fabrika firmware'ine geri dönüş rehberi")
        ]

        for fname, desc in docs_list:
            card = QFrame()
            card.setStyleSheet("background-color: #111827; border: 1px solid #1F2937; border-radius: 6px; padding: 10px; margin-bottom: 6px;")
            c_lay = QHBoxLayout(card)
            f_lbl = QLabel(f"📄 <b>{fname}</b>")
            f_lbl.setStyleSheet("color: #38BDF8; font-size: 13px;")
            d_lbl = QLabel(desc)
            d_lbl.setStyleSheet("color: #94A3B8; font-size: 12px;")
            c_lay.addWidget(f_lbl, 1)
            c_lay.addWidget(d_lbl, 2)
            layout.addWidget(card)

        layout.addStretch()
        return page

    # -------------------------------------------------------------
    # METODLAR VE MANTIK
    # -------------------------------------------------------------
    def update_boot_instruction(self):
        if self.rb_mcu.isChecked():
            instruction = "<b>Tuş Kombinasyonu:</b> Telsiz kapalıyken <b>PTT + Üst Alarm Tuşuna (PF3)</b> basılı tutarak açın. (Kırmızı LED yanıp söner, ekran kararır)."
        elif self.rb_icon.isChecked():
            instruction = "<b>Tuş Kombinasyonu:</b> Telsiz kapalıyken <b>PTT + Alt Yan Tuşa (PF2)</b> basılı tutarak açın. (Ekranda 'UPDATE MODE' yazar)."
        elif self.rb_sct.isChecked():
            instruction = "<b>Tuş Kombinasyonu:</b> Telsiz kapalıyken <b># Tuşu + Üst Tuşa (PF3)</b> basılı tutarak açın. (Ekranda 'WARNING This is Boot Mode for Sct!!!' yazar)."
        else:
            instruction = "<b>Tuş Kombinasyonu:</b> Telsiz kapalıyken <b>Üst Tuş (PF3) + Alt Yan Tuşa (PF2)</b> basılı tutarak açın. (Ekranda 'UPDATE MODE FOR LinkBoard' yazar)."
        self.info_box.setText(
            instruction + "<br><b>Bu sürüm:</b> Yalnızca paket ön kontrolü yapar; hedefe özgü yazma protokolü henüz doğrulanmadı ve cihaza veri gönderilmez."
        )

    def on_screen_selected(self, bmp_name):
        p = os.path.join(self.screens_dir, bmp_name)
        if os.path.exists(p):
            error = self._load_boot_image(p)
            if error:
                QMessageBox.warning(self, "BMP Önizleme Hatası", f"Stok görsel doğrulanamadı:\n{error}")

    def select_custom_bmp(self):
        path, _ = QFileDialog.getOpenFileName(self, "BMP Resmi Seç", self.project_root, "Bitmap (*.bmp)")
        if path:
            error = self._load_boot_image(path)
            if error:
                QMessageBox.warning(self, "Uyumsuz BMP", error)

    def _load_boot_image(self, path):
        try:
            with open(path, "rb") as image_file:
                header = image_file.read(54)
                file_size = os.path.getsize(path)
            if len(header) != 54 or header[:2] != b"BM":
                return "Dosya geçerli bir BMP başlığı içermiyor."
            declared_size = int.from_bytes(header[2:6], "little")
            pixel_offset = int.from_bytes(header[10:14], "little")
            dib_size = int.from_bytes(header[14:18], "little")
            width = int.from_bytes(header[18:22], "little", signed=True)
            height = int.from_bytes(header[22:26], "little", signed=True)
            planes = int.from_bytes(header[26:28], "little")
            bit_depth = int.from_bytes(header[28:30], "little")
            compression = int.from_bytes(header[30:34], "little")
            profile_ok = (
                file_size == 21558 and declared_size == file_size and pixel_offset == 1078
                and dib_size == 40 and width == 160 and height == 128 and planes == 1
                and bit_depth == 8 and compression == 0
            )
            if not profile_ok:
                return (
                    "Beklenen profil: 21.558 byte, 160×128, 8-bit paletli ve sıkıştırmasız BMP. "
                    f"Bulunan: {width}×{height}, {bit_depth}-bit, {file_size} byte."
                )
            reader = QImageReader(path)
            reader.setDecideFormatFromContent(True)
            image = reader.read()
            if image.isNull():
                return f"BMP çözümlenemedi: {reader.errorString()}"
            self.bmp_preview.setPixmap(QPixmap.fromImage(image))
            return ""
        except OSError as exc:
            return f"Dosya okunamadı: {exc}"

    def load_empty_channels(self):
        self.table_channels.setRowCount(0)

    def inspect_codeplug_rdt(self):
        path, _ = QFileDialog.getOpenFileName(
            self, "AnyTone Kod Fişi İncele", self.project_root, "AnyTone Kod Fişi (*.rdt);;Binary Dosya (*.bin);;Tüm Dosyalar (*)"
        )
        if not path:
            return
        try:
            size = os.path.getsize(path)
            if size > 64 * 1024 * 1024:
                raise ValueError("Dosya 64 MiB sınırını aşıyor; incelenmedi.")
            with open(path, "rb") as source:
                data = source.read()
            if not data:
                raise ValueError("Dosya boş.")
            lines = [
                f"Dosya: {os.path.basename(path)}",
                f"Boyut: {size:,} byte",
                f"SHA-256: {hashlib.sha256(data).hexdigest()}",
                "İlk 64 byte:",
            ]
            for offset in range(0, min(len(data), 64), 16):
                block = data[offset:offset + 16]
                hex_text = " ".join(f"{byte:02X}" for byte in block)
                ascii_text = "".join(chr(byte) if 32 <= byte <= 126 else "." for byte in block)
                lines.append(f"{offset:04X}: {hex_text:<47} |{ascii_text}|")
            lines.extend((
                "",
                "RDT alan/kanal şeması bu projede çözülmedi. Dosya yalnızca incelendi; kanal tablosu ve telsiz değiştirilmedi.",
                "Kanal listesi için Studio CSV biçimini kullanın.",
            ))
            QMessageBox.information(self, "Kod Fişi Dosya İncelemesi", "\n".join(lines))
        except (OSError, ValueError) as exc:
            QMessageBox.critical(self, "RDT İnceleme Hatası", str(exc))

    def refresh_ports(self):
        previous_port = self.port_combo.currentData() or self.settings.value("serial/last_port", "")
        self.port_combo.clear()
        try:
            ports = serial.tools.list_ports.comports()
        except Exception as exc:
            self.port_combo.addItem("Port listesi alınamadı", "")
            self.statusBar().showMessage(f"Seri portlar listelenemedi: {exc}", 8000)
            return
        for p in ports:
            self.port_combo.addItem(f"{p.device} ({p.description})", p.device)
        if not ports:
            self.port_combo.addItem("Port bulunamadı", "")
        if previous_port:
            index = self.port_combo.findData(previous_port)
            if index >= 0:
                self.port_combo.setCurrentIndex(index)

    def test_device_handshake(self):
        port = self.port_combo.currentData()
        if not port:
            QMessageBox.warning(self, "Port Seçimi", "Lütfen geçerli bir seri port seçin!")
            return

        if self.handshake_thread and self.handshake_thread.isRunning():
            self.statusBar().showMessage("Bağlantı testi zaten sürüyor.", 4000)
            return
        if self.flash_thread and self.flash_thread.isRunning():
            QMessageBox.warning(self, "İşlem Sürüyor", "Ön kontrol bitmeden bağlantı testi başlatılamaz.")
            return

        self.conn_indicator.setText("● TEST EDİLİYOR...")
        self.conn_indicator.setStyleSheet("color: #FBBF24; font-weight: 700; font-size: 11px;")
        self.statusBar().showMessage(f"AnyTone telsiz ile {port} üzerinden el sıkışılıyor...")
        self.settings.setValue("serial/last_port", port)
        self.btn_connect.setEnabled(False)
        self.btn_ping_flasher.setEnabled(False)
        self.handshake_thread = HandshakeThread(port, 921600)
        self.handshake_thread.result_signal.connect(self.on_handshake_finished)
        self.handshake_thread.start()

    def on_handshake_finished(self, port, ok, message, model, hw_version):
        self.btn_connect.setEnabled(True)
        self.btn_ping_flasher.setEnabled(True)
        if ok:
            self.conn_indicator.setText(f"● BAĞLANDI ({model})")
            self.conn_indicator.setStyleSheet("color: #10B981; font-weight: 700; font-size: 11px;")
            self.statusBar().showMessage(f"[✓] {message}", 8000)
            self.log(f"[✓] {message}")
            QMessageBox.information(
                self, "El Sıkışma Başarılı!",
                f"AnyTone D878 modeli doğrulandı.\n\nPort: {port}\nModel: {model}\nDonanım: {hw_version}\n\n"
                "Bu bağlantı testi kod fişi veya firmware yazmaz."
            )
        else:
            self.conn_indicator.setText("● BAĞLANTI YOK")
            self.conn_indicator.setStyleSheet("color: #EF4444; font-weight: 700; font-size: 11px;")
            self.statusBar().showMessage(f"[!] {message}", 8000)
            self.log(f"[!] {message}")
            QMessageBox.critical(
                self, "Bağlantı Başarısız",
                f"Telsiz ile el sıkışılamadı.\n\nHata detayı: {message}\n\n"
                "USB seri portunun doğru seçildiğini, telsizin açık olduğunu ve resmi CPS gibi başka bir uygulamanın portu kullanmadığını kontrol edin."
            )

    def select_fw_file(self):
        path, _ = QFileDialog.getOpenFileName(self, "Firmware Seç", self.project_root, "Firmware Binary (*.CDD *.bin)")
        if path:
            self.fw_file_path = path
            self.fw_file_label.setText(path)
            self.log(f"[*] Firmware seçildi: {path}")

    def select_stock_fw(self):
        stock_path = os.path.join(self.project_root, "tools", "firmware_analyzer", "D878UVII_V4.00 FW", "D878UVII_V4.00_20250725.CDD")
        if os.path.exists(stock_path):
            self.fw_file_path = stock_path
            self.fw_file_label.setText(stock_path)
            self.log(f"[+] Orijinal V4.00 stok firmware seçildi: {stock_path}")
        else:
            QMessageBox.information(self, "Stok Firmware Bulunamadı", "Stok firmware paketi bu çalışma klasöründe bulunamadı. Firmware Seç ile dosyayı açabilirsiniz.")

    def select_icon_pkg(self):
        icon_path = os.path.join(self.release_dir, "D878UV_ICONV1.23", "D878_ICON_V1.23.CDD")
        if os.path.exists(icon_path):
            self.fw_file_path = icon_path
            self.fw_file_label.setText(icon_path)
            self.rb_icon.setChecked(True)
            self.log(f"[+] Resmi İkon Paketi (3.7 MB) seçildi: {icon_path}")
        else:
            QMessageBox.information(self, "İkon Paketi Bulunamadı", "Resmi ikon paketi bu çalışma klasöründe bulunamadı.")

    def select_linkboard_pkg(self):
        link_path = os.path.join(self.release_dir, "APRS+BT FW V2.00", "V2.00(H)", "BHT_BOARD_H_V2.00_20230417.CDD")
        if os.path.exists(link_path):
            self.fw_file_path = link_path
            self.fw_file_label.setText(link_path)
            self.rb_link.setChecked(True)
            self.log(f"[+] Resmi LinkBoard APRS+BT Firmware seçildi: {link_path}")
        else:
            QMessageBox.information(self, "LinkBoard Paketi Bulunamadı", "LinkBoard firmware paketi bu çalışma klasöründe bulunamadı.")

    def log(self, text):
        self.log_text.append(text)

    def start_flash(self):
        if self.flash_thread and self.flash_thread.isRunning():
            self.statusBar().showMessage("Paket ön kontrolü zaten sürüyor.", 4000)
            return
        if self.handshake_thread and self.handshake_thread.isRunning():
            QMessageBox.warning(self, "İşlem Sürüyor", "Bağlantı testi bitmeden paket ön kontrolü başlatılamaz.")
            return
        if not self.fw_file_path or not os.path.exists(self.fw_file_path):
            QMessageBox.critical(self, "Hata", "Lütfen geçerli bir firmware dosyası seçin!")
            return

        port = self.port_combo.currentData()
        target = "mcu"
        if self.rb_icon.isChecked():
            target = "icons"
        elif self.rb_sct.isChecked():
            target = "sct3258"
        elif self.rb_link.isChecked():
            target = "linkboard"

        self.btn_flash.setEnabled(False)
        self.btn_connect.setEnabled(False)
        self.btn_ping_flasher.setEnabled(False)
        self.progress_bar.setValue(0)
        self.log(f"[*] Güvenli paket ön kontrolü başlatılıyor ({target}); cihaza yazma yapılmayacak.")

        self.flash_thread = FlasherThread(port, 921600, self.fw_file_path, dry_run=True, target=target)
        self.flash_thread.progress_signal.connect(self.on_flash_progress)
        self.flash_thread.finished_signal.connect(self.on_flash_finished)
        self.flash_thread.start()

    def on_flash_progress(self, pct, text):
        self.progress_bar.setValue(pct)
        self.log(f" -> {text}")

    def on_flash_finished(self, success, msg):
        self.btn_flash.setEnabled(True)
        self.btn_ping_flasher.setEnabled(True)
        self.btn_connect.setEnabled(True)
        if success:
            self.log(f"[✓] {msg}")
            QMessageBox.information(self, "Başarılı", msg)
        else:
            self.log(f"[!] HATA: {msg}")
            QMessageBox.critical(self, "Hata Oluştu", msg)

def main():
    app = QApplication(sys.argv)
    window = ATD878Studio()
    window.show()
    sys.exit(app.exec())

if __name__ == "__main__":
    main()
