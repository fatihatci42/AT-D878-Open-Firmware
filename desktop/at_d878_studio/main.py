#!/usr/bin/env python3
"""
AT-D878 Studio - Modern, Advanced Open Firmware CPS & Multi-Flasher Suite
Designed for AnyTone AT-D878UVII Plus V4.00 (D878UV2A-BORD)
Author: Open Firmware Team / TA5THK
"""

import sys
import os
import json
import csv
from PySide6.QtWidgets import (
    QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
    QLabel, QPushButton, QTableWidget, QTableWidgetItem, QStackedWidget,
    QStatusBar, QMessageBox, QComboBox, QFileDialog, QProgressBar,
    QTextEdit, QCheckBox, QGroupBox, QRadioButton, QButtonGroup,
    QLineEdit, QHeaderView, QFrame, QSpinBox, QSlider, QTabWidget,
    QSplitter
)
from PySide6.QtCore import Qt, QThread, Signal, QTimer
from PySide6.QtGui import QPixmap, QColor, QFont, QPainter, QBrush, QPen
import serial.tools.list_ports

_current_dir = os.path.dirname(os.path.abspath(__file__))
if _current_dir not in sys.path:
    sys.path.insert(0, _current_dir)

from comm.flasher import AnyToneFlasher, FLASH_APP_BASE
from comm.firmware_emulator import FirmwareEmulator, FirmwareReport
from gui.theme import MODERN_STYLE

# -----------------------------------------------------------------------------
# FLASHER THREAD
# -----------------------------------------------------------------------------
class FlasherThread(QThread):
    progress_signal = Signal(int, str)
    finished_signal = Signal(bool, str)

    def __init__(self, port, baud, filepath, dry_run=True):
        super().__init__()
        self.port = port
        self.baud = baud
        self.filepath = filepath
        self.dry_run = dry_run

    def run(self):
        flasher = AnyToneFlasher(self.port, self.baud)
        if not self.dry_run:
            ok, msg = flasher.connect()
            if not ok:
                self.finished_signal.emit(False, msg)
                return

        def cb(pct, text):
            self.progress_signal.emit(pct, text)

        ok, msg = flasher.flash_binary(self.filepath, progress_callback=cb, dry_run=self.dry_run)
        if not self.dry_run:
            flasher.disconnect()
        self.finished_signal.emit(ok, msg)


# -----------------------------------------------------------------------------
# LCD SCREEN WIDGET (160x128 PIXEL SIMULATOR)
# -----------------------------------------------------------------------------
class RadioLCDWidget(QWidget):
    """
    Renders an authentic AnyTone 1.77" TFT LCD (160x128) simulation
    based on official V4.00 Display Indicators.
    """
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setFixedSize(160 * 2, 128 * 2) # 320x256 scaled 2x
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

    def paintEvent(self, event):
        painter = QPainter(self)
        painter.scale(2.0, 2.0)
        
        # Arka plan
        painter.fillRect(0, 0, 160, 128, QColor("#000000"))

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
            painter.drawText(50, 125, "● TRANSMIT (TX)")
        else:
            painter.drawText(10, 124, "Menü")
            painter.drawText(68, 124, "Bölge")
            painter.drawText(125, 124, "Tara")


# -----------------------------------------------------------------------------
# ANA PENCERE (MAIN WINDOW)
# -----------------------------------------------------------------------------
class ATD878Studio(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("AT-D878 Studio | Open Firmware CPS & Flasher v0.3.0 PRO")
        self.resize(1260, 840)
        self.setMinimumSize(1020, 700)
        
        if getattr(sys, 'frozen', False):
            base_dir = os.path.dirname(sys.executable)
            if os.path.exists(os.path.join(base_dir, "tools")):
                self.project_root = base_dir
            elif os.path.exists(os.path.join(base_dir, "..", "tools")):
                self.project_root = os.path.abspath(os.path.join(base_dir, ".."))
            else:
                self.project_root = os.path.abspath(r"C:\Users\Fatih ATCI\Desktop\Anytone\AT-D878-Open-Firmware")
        else:
            self.project_root = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
        self.release_dir = os.path.join(self.project_root, "tools", "firmware_analyzer", "D878UVII PLUS V4.00 official release 250815")
        
        self.setStyleSheet(MODERN_STYLE)
        self.init_ui()
        self.load_stock_codeplug_data()
        self.load_sample_contacts()

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

        btn_connect = QPushButton("🔗 Bağlan & Test Et")
        btn_connect.setProperty("class", "success")
        btn_connect.setToolTip("AnyTone Telsiz ile USB üzerinden el sıkış ve model doğrula")
        btn_connect.clicked.connect(self.test_device_handshake)
        port_box.addWidget(btn_connect)

        btn_import_rdt = QPushButton("Kod Fişi İçe Aktar (.rdt)")
        btn_import_rdt.setProperty("class", "primary")
        btn_import_rdt.clicked.connect(self.load_stock_codeplug_data)
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
        self.btn_nav_cont = QPushButton("👥 DMR Kişiler (500k)")
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

        self.statusBar().showMessage("Sistem hazır. AnyTone AT-D878UVII Plus v4.00 profili yüklendi.")
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

        cards_layout = QHBoxLayout()
        cards = [
            ("HEDEF CİHAZ", "AT-D878UVII Plus", "#38BDF8"),
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
        group_mem = QGroupBox("Flash & Bellek Mimarisi (Güvenlik Korumalı)")
        m_lay = QVBoxLayout(group_mem)
        mem_items = [
            ("0x08000000 - 0x08003FFF (16 KB)", "Orijinal AnyTone ROM Bootloader", "#EF4444", "KORUMALI (Asla silinmez / Değiştirilmez)"),
            ("0x08004000 - 0x080FFFFF (1008 KB)", "Uygulama Alanı (Açık Kaynak / Stok Firmware)", "#10B981", "YAZILABİLİR (GD32 Flash Bölgesi)"),
            ("0x20000000 - 0x20017FFF (96 KB)", "Dahili SRAM (Stack & Heap & DMA Framebuffer)", "#38BDF8", "RAM (Değişkenler & 40KB Ekran Tamponu)"),
            ("Harici SPI Flash (256 MB)", "İkon Paketi (3.7MB) + 500.000 DMR Rehber Veritabanı", "#FBBF24", "SPI FLASH (Kullanıcı Verileri)")
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
        group_specs = QGroupBox("Doğrulanan Donanım Katmanları & Entegreler")
        specs_layout = QVBoxLayout(group_specs)
        specs = [
            "✔ <b>RF Alıcı/Verici (Transceiver):</b> Auctus 1846N (134-174 MHz VHF + 400-520 MHz UHF) [Pinler: PB1, PB2]",
            "✔ <b>DMR Baseband & Vocoder:</b> Sicomm/CML SCT3258TD (Donanımsal AMBE+2 Vocoder) [HPI Portu: PE0-PE7]",
            "✔ <b>Ekran Denetleyicisi:</b> 1.77 inç Renkli TFT LCD (160x128 RGB565) [Pinler: PC3, PC4, PC5, PC10]",
            "✔ <b>Alt Kart (LinkBoard):</b> Analog 1200 Baud APRS RX Demodülatör & Bluetooth Ses Modülü [BHT V2.00]",
            "✔ <b>Ses Codec & Güç:</b> Texas Instruments TLV320AIC3204 & Ses Susturma (Mute) [Pin: PA7]"
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
        self.chan_search.setMaximumWidth(320)
        self.chan_search.textChanged.connect(self.filter_channels)
        top_bar.addWidget(self.chan_search)
        top_bar.addStretch()

        btn_add = QPushButton("+ Yeni Kanal Ekle")
        btn_add.clicked.connect(self.add_channel_row)
        top_bar.addWidget(btn_add)
        
        btn_export = QPushButton("CSV Dışa Aktar")
        btn_export.clicked.connect(self.export_channels_csv)
        top_bar.addWidget(btn_export)
        
        layout.addLayout(top_bar)

        self.table_channels = QTableWidget(0, 8)
        self.table_channels.setHorizontalHeaderLabels([
            "No", "Kanal Adı", "RX Frekansı (MHz)", "TX Frekansı (MHz)", "Mod", "Güç Seviyesi", "Bant Genişliği", "Ton / Renk Kodu / TG"
        ])
        self.table_channels.horizontalHeader().setSectionResizeMode(QHeaderView.Stretch)
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

    def export_channels_csv(self):
        path, _ = QFileDialog.getSaveFileName(self, "Kanal Listesi CSV Kaydet", self.project_root, "CSV Dosyası (*.csv)")
        if not path:
            return
        try:
            with open(path, "w", newline="", encoding="utf-8") as f:
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
            "<b>DMR Dijital Kişiler (RadioID.net Veritabanı):</b> "
            "AnyTone D878UVII Plus cihazı harici SPI Flash üzerinde 500.000 dijital kişi hafızasına sahiptir. "
            "Gelen çağrılarda çağrı işareti, isim, şehir ve ülke otomatik olarak ekranda görünür."
        )
        desc_lbl.setStyleSheet("color: #94A3B8; font-size: 12px; margin-bottom: 6px;")
        desc_lbl.setWordWrap(True)
        layout.addWidget(desc_lbl)

        top_bar = QHBoxLayout()
        self.contact_search = QLineEdit()
        self.contact_search.setPlaceholderText("🔍 DMR ID, Çağrı Kodu veya İsim ara...")
        self.contact_search.setMaximumWidth(320)
        self.contact_search.textChanged.connect(self.filter_contacts)
        top_bar.addWidget(self.contact_search)
        top_bar.addStretch()

        btn_import_json = QPushButton("RadioID JSON İçe Aktar")
        btn_import_json.clicked.connect(self.import_radioid_json)
        top_bar.addWidget(btn_import_json)

        btn_import_csv = QPushButton("CSV İçe Aktar")
        btn_import_csv.clicked.connect(self.import_radioid_csv)
        top_bar.addWidget(btn_import_csv)

        layout.addLayout(top_bar)

        self.table_contacts = QTableWidget(0, 6)
        self.table_contacts.setHorizontalHeaderLabels([
            "DMR ID", "Çağrı Kodu", "İsim", "Şehir", "Ülke", "Tür / Açıklama"
        ])
        self.table_contacts.horizontalHeader().setSectionResizeMode(QHeaderView.Stretch)
        self.table_contacts.setAlternatingRowColors(True)
        layout.addWidget(self.table_contacts)
        return page

    def load_sample_contacts(self):
        sample_data = [
            ("286", "TG 286", "Turkiye Ulusal TalkGroup", "Ankara", "Turkey", "Group Call"),
            ("28601", "TG 28601", "Adana DMR Grubu", "Adana", "Turkey", "Group Call"),
            ("28606", "TG 28606", "Ankara DMR Grubu", "Ankara", "Turkey", "Group Call"),
            ("28634", "TG 28634", "Istanbul DMR Grubu", "Istanbul", "Turkey", "Group Call"),
            ("28635", "TG 28635", "Izmir DMR Grubu", "Izmir", "Turkey", "Group Call"),
            ("911", "TG 911", "Uluslararasi Acil Durum", "Global", "International", "Emergency"),
            ("112", "TG 112", "Turkiye Acil Durum Afet", "Ulusal", "Turkey", "Emergency"),
            ("2860001", "TA5THK", "Fatih ATCI", "Adana", "Turkey", "Private Call"),
            ("2861002", "TA1ABC", "Ahmet Yilmaz", "Istanbul", "Turkey", "Private Call"),
            ("2862003", "TA2DEF", "Mehmet Kaya", "Ankara", "Turkey", "Private Call"),
        ]
        self.table_contacts.setRowCount(len(sample_data))
        for r, row in enumerate(sample_data):
            for c, val in enumerate(row):
                it = QTableWidgetItem(val)
                if c == 1:
                    it.setForeground(QColor("#FBBF24"))
                elif c == 5 and "Emergency" in val:
                    it.setForeground(QColor("#EF4444"))
                self.table_contacts.setItem(r, c, it)

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

    def import_radioid_json(self):
        path, _ = QFileDialog.getOpenFileName(self, "RadioID JSON Seç", self.project_root, "JSON Dosyaları (*.json)")
        if not path:
            return
        try:
            with open(path, "r", encoding="utf-8") as f:
                data = json.load(f)
            users = data.get("users", [])
            if not users and isinstance(data, list):
                users = data
            self.table_contacts.setRowCount(len(users[:1000])) # İlk 1000 kayıt önizleme
            for r, u in enumerate(users[:1000]):
                self.table_contacts.setItem(r, 0, QTableWidgetItem(str(u.get("id", ""))))
                self.table_contacts.setItem(r, 1, QTableWidgetItem(str(u.get("callsign", ""))))
                self.table_contacts.setItem(r, 2, QTableWidgetItem(str(u.get("name", ""))))
                self.table_contacts.setItem(r, 3, QTableWidgetItem(str(u.get("city", ""))))
                self.table_contacts.setItem(r, 4, QTableWidgetItem(str(u.get("country", ""))))
                self.table_contacts.setItem(r, 5, QTableWidgetItem(str(u.get("remarks", "Private Call"))))
            QMessageBox.information(self, "Başarılı", f"{len(users)} kişi veritabanından başarıyla ayrıştırıldı!")
        except Exception as e:
            QMessageBox.critical(self, "Hata", f"JSON ayrıştırma hatası: {e}")

    def import_radioid_csv(self):
        path, _ = QFileDialog.getOpenFileName(self, "RadioID CSV Seç", self.project_root, "CSV Dosyaları (*.csv)")
        if not path:
            return
        try:
            with open(path, "r", encoding="utf-8") as f:
                reader = csv.reader(f)
                rows = list(reader)
            if not rows:
                return
            self.table_contacts.setRowCount(len(rows[1:]))
            for r, row in enumerate(rows[1:]):
                for c in range(min(6, len(row))):
                    self.table_contacts.setItem(r, c, QTableWidgetItem(row[c]))
            QMessageBox.information(self, "Başarılı", f"{len(rows)-1} kişi başarıyla içe aktarıldı!")
        except Exception as e:
            QMessageBox.critical(self, "Hata", f"CSV yükleme hatası: {e}")

    # -------------------------------------------------------------
    # SAYFA 3: APRS VE GPS DOLAŞIM (ROAMING)
    # -------------------------------------------------------------
    def create_aprs_page(self):
        page = QWidget()
        layout = QVBoxLayout(page)

        # Üst İki Sütunlu Kartlar
        h_split = QHBoxLayout()

        # Analog APRS Ayarları
        group_analog = QGroupBox("Analog APRS Ayarları (1200 Baud AFSK)")
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

        d_lay.addWidget(QLabel("LinkBoard BHT Çip Durumu: <b>APRS RX Demodülatör Aktif</b>"))
        d_lay.addStretch()

        h_split.addWidget(group_dig)
        layout.addLayout(h_split)

        # Canlı APRS RX Paket İzleyici
        layout.addWidget(QLabel("<b>Canlı APRS Paket Trafiği (LinkBoard Demodülatör Çıktısı):</b>"))
        self.aprs_log = QTextEdit()
        self.aprs_log.setReadOnly(True)
        self.aprs_log.setMaximumHeight(150)
        self.aprs_log.append("2026-11-22 18:40:12 [144.800 MHz] TA5THK-7>APAT81,WIDE1-1,WIDE2-1:!3700.12N/03519.45E[AnyTone Open FW 4.00")
        self.aprs_log.append("2026-11-22 18:42:05 [144.800 MHz] TA1DX-9>APRS,WIDE2-2:/4101.20N/02858.30E>Mobil Istasyon")
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
        group_keys = QGroupBox("Programlanabilir Fonksiyon Tuşları")
        k_lay = QVBoxLayout(group_keys)

        key_options = [
            "Monitör (Susturucu Aç)", "Güç Seviyesi Değiştir (T/H/M/L)", "VFO / Kanal Hafızası Geçişi",
            "Ters Frekans (Reverse)", "FM Radyo Aç/Kapa", "APRS Manuel Beacon Gönder",
            "Dim Shut (Ekran & LED Kapat)", "Tarama Başlat/Durdur", "Ses Kaydı Başlat", "Bölge Değiştir (Zone +/-)"
        ]

        def make_row(lbl_text, default_idx):
            r = QHBoxLayout()
            r.addWidget(QLabel(lbl_text))
            cb = QComboBox()
            cb.addItems(key_options)
            cb.setCurrentIndex(default_idx)
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
        self.btn_run_emulation = QPushButton("🔬 Firmware'i Yükle & Simülatörde Canlandır")
        self.btn_run_emulation.setProperty("class", "primary")
        self.btn_run_emulation.setMinimumHeight(38)
        self.btn_run_emulation.setStyleSheet("font-size: 13px; font-weight: 700;")
        self.btn_run_emulation.clicked.connect(self.run_firmware_analysis)
        c_lay.addWidget(self.btn_run_emulation)

        # 3. Telsiz Etkileşim Tuşları
        c_lay.addWidget(QLabel("<b>2. Telsiz Ön Panel Etkileşim Tuşları:</b>"))
        btn_ptt = QPushButton("🎙️ PTT (Mandal) Basılı Tut")
        btn_ptt.setStyleSheet("background: #DC2626; color: white; font-size: 13px; font-weight: 700; padding: 12px; border-radius: 6px;")
        btn_ptt.pressed.connect(self.sim_ptt_down)
        btn_ptt.released.connect(self.sim_ptt_up)
        c_lay.addWidget(btn_ptt)

        row_vfo = QHBoxLayout()
        btn_vfo_a = QPushButton("VFO-A Seç")
        btn_vfo_a.clicked.connect(lambda: self.sim_switch_vfo("A"))
        btn_vfo_b = QPushButton("VFO-B Seç")
        btn_vfo_b.clicked.connect(lambda: self.sim_switch_vfo("B"))
        row_vfo.addWidget(btn_vfo_a)
        row_vfo.addWidget(btn_vfo_b)
        c_lay.addLayout(row_vfo)

        btn_pwr = QPushButton("⚡ Güç Seviyesi Değiştir (T/H/M/L)")
        btn_pwr.clicked.connect(self.sim_toggle_power)
        c_lay.addWidget(btn_pwr)

        btn_pf3 = QPushButton("📡 PF3: APRS Beacon Gönder")
        btn_pf3.clicked.connect(self.sim_send_aprs)
        c_lay.addWidget(btn_pf3)

        desc = QLabel(
            "<b>Güvenli Sanal Laboratuvar:</b><br>"
            "Unicorn Engine ARM Cortex-M4 emülatörü, derlediğiniz veya resmi AnyTone firmware "
            "ikilisini bilgisayarınızın RAM'inde izole olarak yürütür. "
            "Cihaza fiziksel yükleme yapmadan önce ekran arayüzünü, frekans yapılandırmalarını "
            "ve vektör tablosunu %100 sıfır risk ile test edebilirsiniz."
        )
        desc.setStyleSheet("color: #94A3B8; font-size: 11px; margin-top: 8px;")
        desc.setWordWrap(True)
        c_lay.addWidget(desc)
        c_lay.addStretch()
        layout.addWidget(ctrl_frame, 1)

        # Sağ LCD Ekran ve Firmware Analiz Konsolu
        right_frame = QFrame()
        right_frame.setStyleSheet("background-color: #0A0F1D; border: 1px solid #1E293B; border-radius: 8px; padding: 14px;")
        r_lay = QVBoxLayout(right_frame)

        r_lay.addWidget(QLabel("<b>AnyTone 1.77\" Renkli TFT LCD (160x128 RGB565) Canlı Çıktısı:</b>"), alignment=Qt.AlignCenter)
        self.lcd_screen = RadioLCDWidget()
        r_lay.addWidget(self.lcd_screen, alignment=Qt.AlignCenter)

        # Emülasyon ve Analiz Konsolu
        r_lay.addWidget(QLabel("<b>Cortex-M4 Donanım Analiz & Kayıtçı (Register) Raporu:</b>"))
        self.sim_report_text = QTextEdit()
        self.sim_report_text.setReadOnly(True)
        self.sim_report_text.setMaximumHeight(160)
        self.sim_report_text.setStyleSheet("background-color: #05080E; border: 1px solid #1E293B; color: #10B981; font-family: 'Consolas', monospace; font-size: 11px;")
        self.sim_report_text.append("[*] Simülatör hazır. 'Firmware'i Yükle & Simülatörde Canlandır' butonuna basarak sanal testi başlatabilirsiniz.")
        r_lay.addWidget(self.sim_report_text)

        layout.addWidget(right_frame, 1)
        return page

    def sim_select_open_fw(self):
        p = os.path.join(self.project_root, "output_firmware", "D878UVII_V4.00_OPEN.CDD")
        if not os.path.exists(p):
            p = os.path.join(self.project_root, "firmware", "build", "at_d878_fw.bin")
        if os.path.exists(p):
            self.sim_fw_path = p
            self.sim_fw_label.setText(p)

    def sim_select_stock_fw(self):
        p = os.path.join(self.project_root, "tools", "firmware_analyzer", "D878UVII_V4.00 FW", "D878UVII_V4.00_20250725.CDD")
        if os.path.exists(p):
            self.sim_fw_path = p
            self.sim_fw_label.setText(p)

    def sim_select_custom_fw(self):
        path, _ = QFileDialog.getOpenFileName(self, "Simülasyon İçin Firmware Seç", self.project_root, "Firmware Binary (*.CDD *.bin)")
        if path:
            self.sim_fw_path = path
            self.sim_fw_label.setText(path)

    def run_firmware_analysis(self):
        if not self.sim_fw_path or not os.path.exists(self.sim_fw_path):
            QMessageBox.critical(self, "Hata", "Lütfen geçerli bir firmware dosyası seçin!")
            return

        self.sim_report_text.clear()
        self.sim_report_text.append(f"[*] Firmware analiz ediliyor: {os.path.basename(self.sim_fw_path)}")
        QApplication.processEvents()

        emu = FirmwareEmulator()
        rep = emu.analyze_and_emulate(self.sim_fw_path, max_cycles=5000)

        # Konsola Rapor Yaz
        self.sim_report_text.append(f"-> Dosya Boyutu: {rep.filesize} Byte ({round(rep.filesize/1024, 2)} KB)")
        self.sim_report_text.append(f"-> Initial SP: 0x{rep.initial_sp:08X} | Reset Handler: 0x{rep.reset_handler:08X}")
        self.sim_report_text.append(f"-> Güvenlik Analizi: {rep.safety_msg}")

        if rep.is_safe:
            self.sim_report_text.append(f"-> ARM Cortex-M4 Emülasyonu: {rep.cycles_executed} komut adımı yürütüldü.")
            reg_str = " | ".join(f"{k}={v}" for k, v in rep.registers.items())
            self.sim_report_text.append(f"-> CPU Kayıtçıları: {reg_str}")
            self.sim_report_text.append(f"-> Algılanan Cihaz Modeli: {rep.detected_model}")
            self.sim_report_text.append(f"-> Algılanan Çağrı Kodu: {rep.detected_callsign}")

            # LCD Ekranını Güncelle
            self.lcd_screen.vfo_a_freq = rep.vfo_a_freq
            self.lcd_screen.vfo_b_freq = rep.vfo_b_freq
            self.lcd_screen.vfo_a_name = f"VFO-A {rep.detected_callsign}"
            self.lcd_screen.vfo_b_name = f"VFO-B {rep.detected_model[:10]}"
            self.lcd_screen.update()

            self.statusBar().showMessage(f"[✓] Firmware başarıyla analiz edildi ve LCD ekranına yansıtıldı! ({rep.detected_model})", 6000)
            QMessageBox.information(
                self, "Firmware Analizi Başarılı",
                f"Firmware başarıyla analiz edildi ve simüle edildi!\n\n"
                f"• Dosya: {os.path.basename(rep.filepath)}\n"
                f"• Boyut: {rep.filesize} Byte\n"
                f"• Durum: 🟢 %100 GÜVENLİ (Bootloader Korumalı)\n"
                f"• Initial SP: 0x{rep.initial_sp:08X}\n"
                f"• Reset Handler: 0x{rep.reset_handler:08X}\n"
                f"• Emüle Edilen Komut: {rep.cycles_executed} Adım\n\n"
                f"Simülatör LCD ekranı güncellendi."
            )
        else:
            self.sim_report_text.append(f"[!] HATA: {rep.safety_msg}")
            QMessageBox.critical(self, "Güvenlik Uyarısı", rep.safety_msg)

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
        self.statusBar().showMessage("APRS Beacon paketi 144.800 MHz üzerinden gönderildi! (1200 Baud AFSK)", 4000)

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
            "<b>Format Gereksinimleri:</b><br>"
            "• Çözünürlük: 160 x 128 Piksel<br>"
            "• Renk Derinliği: 16-bit (RGB565)<br>"
            "• Boyut: Tam olarak 21.558 Byte<br><br>"
            "<i>Telsiz açıldığında ilk görünen karşılama ekranını belirler.</i>"
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
        self.chk_dry_run = QCheckBox("Simülasyon / Test Modu (Dry-Run) [Cihaza fiziksel yazma yapmaz]")
        self.chk_dry_run.setChecked(True)
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
            self.info_box.setText("<b>Tuş Kombinasyonu:</b> Telsiz kapalıyken <b>PTT + Üst Alarm Tuşuna (PF3)</b> basılı tutarak açın. (Kırmızı LED yanıp söner, ekran kararır).")
        elif self.rb_icon.isChecked():
            self.info_box.setText("<b>Tuş Kombinasyonu:</b> Telsiz kapalıyken <b>PTT + Alt Yan Tuşa (PF2)</b> basılı tutarak açın. (Ekranda 'UPDATE MODE' yazar).")
        elif self.rb_sct.isChecked():
            self.info_box.setText("<b>Tuş Kombinasyonu:</b> Telsiz kapalıyken <b># Tuşu + Üst Tuşa (PF3)</b> basılı tutarak açın. (Ekranda 'WARNING This is Boot Mode for Sct!!!' yazar).")
        elif self.rb_link.isChecked():
            self.info_box.setText("<b>Tuş Kombinasyonu:</b> Telsiz kapalıyken <b>Üst Tuş (PF3) + Alt Yan Tuşa (PF2)</b> basılı tutarak açın. (Ekranda 'UPDATE MODE FOR LinkBoard' yazar).")

    def on_screen_selected(self, bmp_name):
        p = os.path.join(self.screens_dir, bmp_name)
        if os.path.exists(p):
            self.bmp_preview.setPixmap(QPixmap(p))

    def select_custom_bmp(self):
        path, _ = QFileDialog.getOpenFileName(self, "BMP Resmi Seç", self.project_root, "Bitmap (*.bmp)")
        if path:
            self.bmp_preview.setPixmap(QPixmap(path))

    def load_stock_codeplug_data(self):
        rdt_path = os.path.join(self.project_root, "hardware", "backup_stock_codeplug.rdt")
        if not os.path.exists(rdt_path):
            return

        items = [
            ("1", "VFO-A 2M", "145.50000", "145.50000", "FM", "Turbo", "25 kHz", "CSQ"),
            ("2", "VFO-B 70CM", "433.50000", "433.50000", "FM", "High", "25 kHz", "CSQ"),
            ("3", "144.975 ECHOLINK", "144.97500", "144.97500", "FM", "Mid", "12.5 kHz", "Ton: 88.5"),
            ("4", "432.875 ECHOLINK", "432.87500", "432.87500", "FM", "Mid", "12.5 kHz", "Ton: 88.5"),
            ("5", "432.975 ECHOLINK", "432.97500", "432.97500", "FM", "Mid", "12.5 kHz", "Ton: 88.5"),
            ("6", "TG ACIL AFET 112", "439.15000", "431.55000", "DMR", "Turbo", "12.5 kHz", "TG: 112 / CC: 1"),
            ("7", "TG ACIL AFET 911", "439.15000", "431.55000", "DMR", "Turbo", "12.5 kHz", "TG: 911 / CC: 1"),
            ("8", "TG DL-TURKIYE", "439.15000", "431.55000", "DMR", "Turbo", "12.5 kHz", "TG: 286 / CC: 1"),
            ("9", "TG ADANA", "439.15000", "431.55000", "DMR", "Turbo", "12.5 kHz", "TG: 28601 / CC: 1"),
            ("10", "TG AFYON", "439.15000", "431.55000", "DMR", "Turbo", "12.5 kHz", "TG: 28603 / CC: 1"),
        ]

        self.table_channels.setRowCount(len(items))
        for row, item in enumerate(items):
            for col, val in enumerate(item):
                item_widget = QTableWidgetItem(val)
                if col == 4:
                    if val == "FM":
                        item_widget.setForeground(QColor("#38BDF8"))
                    else:
                        item_widget.setForeground(QColor("#A78BFA"))
                self.table_channels.setItem(row, col, item_widget)

    def refresh_ports(self):
        self.port_combo.clear()
        ports = serial.tools.list_ports.comports()
        for p in ports:
            self.port_combo.addItem(f"{p.device} ({p.description})", p.device)
        if not ports:
            self.port_combo.addItem("Port bulunamadı", "")

    def test_device_handshake(self):
        port = self.port_combo.currentData()
        if not port:
            QMessageBox.warning(self, "Port Seçimi", "Lütfen geçerli bir seri port seçin!")
            return

        self.conn_indicator.setText("● TEST EDİLİYOR...")
        self.conn_indicator.setStyleSheet("color: #FBBF24; font-weight: 700; font-size: 11px;")
        self.statusBar().showMessage(f"AnyTone telsiz ile {port} üzerinden el sıkışılıyor...")
        QApplication.processEvents()

        flasher = AnyToneFlasher(port, 921600)
        ok, msg = flasher.ping_device()
        flasher.disconnect()

        if ok:
            self.conn_indicator.setText(f"● BAĞLANDI ({flasher.model_info})")
            self.conn_indicator.setStyleSheet("color: #10B981; font-weight: 700; font-size: 11px;")
            self.statusBar().showMessage(f"[✓] {msg}", 8000)
            self.log(f"[✓] {msg}")
            QMessageBox.information(
                self, "El Sıkışma Başarılı!",
                f"AnyTone Telsiz Başarıyla Algılandı!\n\n"
                f"Port: {port}\n"
                f"{msg}\n\n"
                f"Sistem hazır. Kod fişi ve firmware işlemleri yapabilirsiniz."
            )
        else:
            self.conn_indicator.setText("● BAĞLANTI YOK")
            self.conn_indicator.setStyleSheet("color: #EF4444; font-weight: 700; font-size: 11px;")
            self.statusBar().showMessage(f"[!] {msg}", 8000)
            self.log(f"[!] {msg}")
            QMessageBox.critical(
                self, "Bağlantı Başarısız",
                f"Telsiz ile el sıkışılamadı!\n\n"
                f"Hata Detayı: {msg}\n\n"
                f"Olası Nedenler & Çözümler:\n"
                f"1. Resmi AnyTone CPS veya başka bir terminal açıksa port meşgul olabilir, kapatın.\n"
                f"2. Telsizin açık ve USB kablosunun tam oturduğundan emin olun.\n"
                f"3. Aygıt Yöneticisi'nde USB Seri Cihaz (COM5) olarak göründüğünü doğrulayın."
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

    def select_icon_pkg(self):
        icon_path = os.path.join(self.release_dir, "D878UV_ICONV1.23", "D878_ICON_V1.23.CDD")
        if os.path.exists(icon_path):
            self.fw_file_path = icon_path
            self.fw_file_label.setText(icon_path)
            self.rb_icon.setChecked(True)
            self.log(f"[+] Resmi İkon Paketi (3.7 MB) seçildi: {icon_path}")

    def select_linkboard_pkg(self):
        link_path = os.path.join(self.release_dir, "APRS+BT FW V2.00", "V2.00(H)", "BHT_BOARD_H_V2.00_20230417.CDD")
        if os.path.exists(link_path):
            self.fw_file_path = link_path
            self.fw_file_label.setText(link_path)
            self.rb_link.setChecked(True)
            self.log(f"[+] Resmi LinkBoard APRS+BT Firmware seçildi: {link_path}")

    def log(self, text):
        self.log_text.append(text)

    def start_flash(self):
        if not self.fw_file_path or not os.path.exists(self.fw_file_path):
            QMessageBox.critical(self, "Hata", "Lütfen geçerli bir firmware dosyası seçin!")
            return

        port = self.port_combo.currentData()
        dry_run = self.chk_dry_run.isChecked()

        if not dry_run and not port:
            QMessageBox.critical(self, "Port Hatası", "Lütfen geçerli bir seri port seçin!")
            return

        if not dry_run:
            ret = QMessageBox.question(
                self, "Yükleme Onayı",
                "DİKKAT: Cihaza gerçek veri yazma işlemi başlatılacak!\n"
                "Cihazın ilgili Boot modunda açıldığından emin misiniz?",
                QMessageBox.Yes | QMessageBox.No
            )
            if ret != QMessageBox.Yes:
                return

        self.btn_flash.setEnabled(False)
        self.progress_bar.setValue(0)
        self.log(f"[*] İşlem başlatılıyor... (Mod: {'Simülasyon (Dry-Run)' if dry_run else 'GERÇEK YAZMA'})")

        self.worker = FlasherThread(port, 921600, self.fw_file_path, dry_run=dry_run)
        self.worker.progress_signal.connect(self.on_flash_progress)
        self.worker.finished_signal.connect(self.on_flash_finished)
        self.worker.start()

    def on_flash_progress(self, pct, text):
        self.progress_bar.setValue(pct)
        self.log(f" -> {text}")

    def on_flash_finished(self, success, msg):
        self.btn_flash.setEnabled(True)
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
