"""
Modern High-Tech Dark Theme QSS Stylesheet for AT-D878 Studio
Crafted for AnyTone AT-D878UVII Plus Open Firmware Suite
"""

MODERN_STYLE = """
/* Ana Pencere ve Genel Tipografi */
QMainWindow {
    background-color: #0A0E17;
}

QWidget {
    color: #E2E8F0;
    font-family: 'Segoe UI', -apple-system, BlinkMacSystemFont, Roboto, sans-serif;
    font-size: 13px;
}

/* 1. ÜST HEADER ALANI */
#header {
    background-color: #111827;
    border-bottom: 1px solid #1F2937;
    padding: 8px 18px;
}

#app_title {
    font-size: 18px;
    font-weight: 800;
    letter-spacing: 0.5px;
    color: #F8FAFC;
}

#app_badge {
    background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #0284C7, stop:1 #0369A1);
    color: #FFFFFF;
    font-size: 10px;
    font-weight: 700;
    letter-spacing: 1px;
    padding: 3px 8px;
    border-radius: 4px;
}

#hw_badge {
    background-color: #1E293B;
    color: #38BDF8;
    border: 1px solid #0284C7;
    font-size: 11px;
    font-weight: 600;
    padding: 2px 7px;
    border-radius: 4px;
}

/* 2. SOL KENAR ÇUBUĞU (SIDEBAR) */
#sidebar {
    background-color: #0F172A;
    border-right: 1px solid #1E293B;
    min-width: 230px;
    max-width: 230px;
}

#sidebar QPushButton {
    background-color: transparent;
    color: #94A3B8;
    text-align: left;
    padding: 11px 18px;
    font-size: 13px;
    font-weight: 600;
    border: none;
    border-left: 4px solid transparent;
    border-radius: 0px;
}

#sidebar QPushButton:hover {
    background-color: #1E293B;
    color: #F8FAFC;
}

#sidebar QPushButton:checked {
    background-color: #1E293B;
    color: #38BDF8;
    font-weight: 700;
    border-left: 4px solid #38BDF8;
}

/* 3. KARTLAR VE PANELLER */
QGroupBox {
    background-color: #111827;
    border: 1px solid #1F2937;
    border-radius: 8px;
    margin-top: 14px;
    padding: 16px;
    font-weight: 700;
}

QGroupBox::title {
    subcontrol-origin: margin;
    subcontrol-position: top left;
    padding: 0 10px;
    color: #38BDF8;
    font-size: 12px;
    letter-spacing: 0.5px;
}

.stat-card {
    background-color: #111827;
    border: 1px solid #1F2937;
    border-radius: 8px;
    padding: 14px;
}

/* 4. BUTONLAR */
QPushButton {
    background-color: #1E293B;
    color: #F8FAFC;
    border: 1px solid #334155;
    border-radius: 6px;
    padding: 7px 16px;
    font-weight: 600;
    font-size: 12px;
}

QPushButton:hover {
    background-color: #334155;
    border-color: #475569;
}

QPushButton:pressed {
    background-color: #0F172A;
}

QPushButton:disabled {
    background-color: #0F172A;
    color: #475569;
    border-color: #1E293B;
}

QPushButton.primary {
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #0284C7, stop:1 #0369A1);
    color: #FFFFFF;
    border: 1px solid #0284C7;
}

QPushButton.primary:hover {
    background: #0284C7;
}

QPushButton.success {
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #10B981, stop:1 #059669);
    color: #FFFFFF;
    border: 1px solid #10B981;
}

QPushButton.success:hover {
    background: #10B981;
}

QPushButton.danger {
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #EF4444, stop:1 #DC2626);
    color: #FFFFFF;
    border: 1px solid #EF4444;
}

QPushButton.danger:hover {
    background: #EF4444;
}

/* 5. GİRİŞ KONTROLLERİ */
QLineEdit, QComboBox, QSpinBox {
    background-color: #0B0F17;
    border: 1px solid #2D3748;
    border-radius: 6px;
    padding: 6px 12px;
    color: #F8FAFC;
    selection-background-color: #0284C7;
}

QLineEdit:focus, QComboBox:focus, QSpinBox:focus {
    border: 1px solid #38BDF8;
    background-color: #0F172A;
}

QComboBox::drop-down {
    subcontrol-origin: padding;
    subcontrol-position: top right;
    width: 24px;
    border-left: 1px solid #2D3748;
}

QComboBox QAbstractItemView {
    background-color: #111827;
    border: 1px solid #374151;
    selection-background-color: #0284C7;
    selection-color: #FFFFFF;
    color: #F8FAFC;
    padding: 4px;
}

/* 6. TABLOLAR (CHANNELS & CONTACTS) */
QTableWidget {
    background-color: #0B0F17;
    border: 1px solid #1F2937;
    border-radius: 6px;
    gridline-color: #1A2332;
    selection-background-color: #0369A1;
    selection-color: #FFFFFF;
    font-size: 12px;
}

QTableWidget::item {
    padding: 6px 10px;
    border-bottom: 1px solid #16202E;
}

QTableWidget::item:selected {
    background-color: #1E3A5F;
    color: #FFFFFF;
}

QHeaderView::section {
    background-color: #111827;
    color: #94A3B8;
    font-weight: 700;
    font-size: 11px;
    letter-spacing: 0.5px;
    padding: 8px 10px;
    border: none;
    border-bottom: 2px solid #1F2937;
}

/* 7. RADYO DÜĞMELERİ VE ONAY KUTULARI */
QRadioButton, QCheckBox {
    spacing: 8px;
    font-weight: 500;
    color: #CBD5E1;
}

QRadioButton::indicator, QCheckBox::indicator {
    width: 16px;
    height: 16px;
    background-color: #0B0F17;
    border: 1px solid #475569;
    border-radius: 4px;
}

QRadioButton::indicator {
    border-radius: 8px;
}

QRadioButton::indicator:checked, QCheckBox::indicator:checked {
    background-color: #0284C7;
    border-color: #38BDF8;
}

/* 8. İLERLEME ÇUBUĞU */
QProgressBar {
    background-color: #0B0F17;
    border: 1px solid #1F2937;
    border-radius: 6px;
    text-align: center;
    color: #F8FAFC;
    font-weight: 700;
    font-size: 11px;
    height: 20px;
}

QProgressBar::chunk {
    background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #0284C7, stop:1 #10B981);
    border-radius: 5px;
}

/* 9. METİN ALANI / TERMİNAL */
QTextEdit {
    background-color: #05080E;
    border: 1px solid #1E293B;
    border-radius: 6px;
    color: #10B981;
    font-family: 'Consolas', 'Cascadia Code', monospace;
    font-size: 12px;
    padding: 8px;
}

/* 10. KAYDIRMA ÇUBUKLARI */
QScrollBar:vertical {
    border: none;
    background-color: #0B0F17;
    width: 8px;
    margin: 0px;
}

QScrollBar::handle:vertical {
    background-color: #334155;
    min-height: 20px;
    border-radius: 4px;
}

QScrollBar::handle:vertical:hover {
    background-color: #475569;
}

QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
    height: 0px;
}

/* 11. DURUM ÇUBUĞU */
QStatusBar {
    background-color: #0B0F17;
    color: #94A3B8;
    border-top: 1px solid #1F2937;
    font-size: 11px;
    padding: 4px 10px;
}
"""
