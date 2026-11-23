"""
WebCPS (cps.aes.app) and AnyTone Official CPS Package Integration Module
Supports importing, parsing, and exporting AnyTone / WebCPS CSV archives.
"""

import os
import io
import csv
import json
import zipfile
import urllib.request
from typing import Dict, List, Any, Optional, Tuple


class WebCPSPackage:
    def __init__(self):
        self.project_name: str = ""
        self.channels: List[List[str]] = []
        self.zones: List[Dict[str, Any]] = []
        self.talkgroups: List[List[str]] = []
        self.fm_stations: List[Dict[str, Any]] = []
        self.settings: Dict[str, str] = {}

    @property
    def summary(self) -> str:
        return (
            f"Kanal Sayısı: {len(self.channels)}\n"
            f"Bölge (Zone) Sayısı: {len(self.zones)}\n"
            f"DMR TalkGroup Sayısı: {len(self.talkgroups)}\n"
            f"FM Radyo İstasyonu: {len(self.fm_stations)}"
        )


def _normalize_header(value: str) -> str:
    return "".join(ch for ch in value.casefold() if ch.isalnum())


def parse_webcps_zip(zip_source) -> WebCPSPackage:
    """
    Parse a WebCPS or AnyTone CPS export ZIP archive (file path or bytes).
    """
    pkg = WebCPSPackage()

    if isinstance(zip_source, (str, bytes, os.PathLike)) and isinstance(zip_source, str) and os.path.isfile(zip_source):
        zf = zipfile.ZipFile(zip_source, 'r')
    elif isinstance(zip_source, bytes):
        zf = zipfile.ZipFile(io.BytesIO(zip_source), 'r')
    elif hasattr(zip_source, 'read'):
        zf = zipfile.ZipFile(zip_source, 'r')
    else:
        raise ValueError("Geçersiz zip veri kaynağı.")

    with zf:
        namelist = zf.namelist()
        lower_names = {n.lower(): n for n in namelist}

        # 1. Parse Channel.CSV
        chan_file = lower_names.get('channel.csv')
        if chan_file:
            with zf.open(chan_file) as f:
                reader = csv.DictReader(io.TextIOWrapper(f, encoding='utf-8-sig', errors='ignore'))
                for r in reader:
                    raw_type = r.get('Channel Type', '').upper().strip()
                    mode = 'FM'
                    if 'DIGITAL' in raw_type or raw_type == 'DMR':
                        mode = 'DMR'

                    if mode == 'DMR':
                        cc = r.get('Color Code', '1')
                        ts = r.get('Slot', '1')
                        contact = r.get('Contact', '')
                        info = f"CC:{cc} | TS{ts} | {contact}"
                    else:
                        enc = r.get('CTCSS/DCS Encode', 'Off')
                        info = f"CTCSS: {enc}" if enc and enc != 'Off' else "CSQ"

                    rx_str = r.get('Receive Frequency', '145.00000').strip()
                    tx_str = r.get('Transmit Frequency', rx_str).strip()
                    try:
                        rx_val = float(rx_str.replace(',', '.'))
                        tx_val = float(tx_str.replace(',', '.'))
                        rx_str = f"{rx_val:.5f}"
                        tx_str = f"{tx_val:.5f}"
                    except ValueError:
                        pass

                    pkg.channels.append([
                        r.get('No.', str(len(pkg.channels) + 1)).strip(),
                        r.get('Channel Name', 'CH').strip()[:16],
                        rx_str,
                        tx_str,
                        mode,
                        r.get('Transmit Power', 'High').strip(),
                        r.get('Band Width', '12.5K').strip(),
                        info
                    ])

        # 2. Parse Zone.CSV
        zone_file = lower_names.get('zone.csv')
        if zone_file:
            with zf.open(zone_file) as f:
                reader = csv.DictReader(io.TextIOWrapper(f, encoding='utf-8-sig', errors='ignore'))
                for r in reader:
                    member_str = r.get('Zone Channel Member', '')
                    channels = [c.strip() for c in member_str.split('|') if c.strip()]
                    pkg.zones.append({
                        'no': r.get('No.', str(len(pkg.zones) + 1)).strip(),
                        'name': r.get('Zone Name', f"Zone {len(pkg.zones)+1}").strip()[:16],
                        'channels': channels,
                        'a_chan': r.get('A Channel', ''),
                        'b_chan': r.get('B Channel', '')
                    })

        # 3. Parse TalkGroups.CSV or DigitalContactList.CSV
        tg_file = lower_names.get('talkgroups.csv') or lower_names.get('digitalcontactlist.csv')
        if tg_file:
            with zf.open(tg_file) as f:
                reader = csv.DictReader(io.TextIOWrapper(f, encoding='utf-8-sig', errors='ignore'))
                for r in reader:
                    dmr_id = r.get('Radio ID', '') or r.get('DMR ID', '') or r.get('Contact TG/DMR ID', '')
                    name = r.get('Name', '') or r.get('Callsign', '')
                    call_type = r.get('Call Type', 'Group Call')
                    if dmr_id:
                        pkg.talkgroups.append([
                            dmr_id.strip(),
                            name.strip(),
                            name.strip(),
                            'TURKIYE',
                            'TR',
                            f"{call_type} (TG {dmr_id.strip()})"
                        ])

        # 4. Parse FM.CSV
        fm_file = lower_names.get('fm.csv')
        if fm_file:
            with zf.open(fm_file) as f:
                reader = csv.DictReader(io.TextIOWrapper(f, encoding='utf-8-sig', errors='ignore'))
                for r in reader:
                    pkg.fm_stations.append({
                        'no': r.get('No.', str(len(pkg.fm_stations) + 1)).strip(),
                        'freq_mhz': r.get('Frequency[MHz]', '').strip(),
                        'scan': r.get('Scan', 'Add').strip()
                    })

        # 5. Parse OptionalSetting.CSV
        opt_file = lower_names.get('optionalsetting.csv')
        if opt_file:
            with zf.open(opt_file) as f:
                reader = csv.reader(io.TextIOWrapper(f, encoding='utf-8-sig', errors='ignore'))
                for row in reader:
                    if len(row) >= 2:
                        pkg.settings[row[0].strip()] = row[1].strip()

    return pkg


def fetch_webcps_project(project_url_or_uuid: str) -> WebCPSPackage:
    """
    Fetch a project from cps.aes.app by URL or UUID and return parsed WebCPSPackage.
    """
    import re
    # Extract UUID if full URL passed
    m = re.search(r'[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}', project_url_or_uuid)
    if not m:
        raise ValueError("Geçersiz WebCPS proje URL veya UUID'si.")
    uuid = m.group(0)

    api_url = f"https://cps.aes.app/api/projects/{uuid}/export/csv"
    req = urllib.request.Request(
        api_url,
        headers={'User-Agent': 'Mozilla/5.0 (AT-D878-Studio/1.0)'}
    )
    with urllib.request.urlopen(req, timeout=15) as resp:
        zip_bytes = resp.read()

    pkg = parse_webcps_zip(zip_bytes)
    pkg.project_name = f"WebCPS-{uuid[:8]}"
    return pkg


def load_turkey_template() -> WebCPSPackage:
    """
    Load the bundled TA1UAT / TA5THK Turkey Amateur Repeaters codeplug.
    """
    pkg = WebCPSPackage()
    data_dir = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "data")
    template_path = os.path.join(data_dir, "turkey_codeplug_template.json")

    if not os.path.isfile(template_path):
        raise FileNotFoundError(f"Türkiye kodplug şablon dosyası bulunamadı: {template_path}")

    with open(template_path, "r", encoding="utf-8") as f:
        data = json.load(f)

    pkg.project_name = data.get("name", "Türkiye Röle Şablonu")
    pkg.channels = data.get("channels", [])
    pkg.zones = data.get("zones", [])
    pkg.talkgroups = data.get("talkgroups", [])
    pkg.fm_stations = data.get("fm", [])
    return pkg


def export_webcps_zip(channels: List[List[str]], zones: List[Dict[str, Any]], talkgroups: List[List[str]], output_path: str):
    """
    Export studio channel, zone, and talkgroup lists into a valid AnyTone CSV ZIP archive.
    """
    with zipfile.ZipFile(output_path, 'w', compression=zipfile.ZIP_DEFLATED) as zf:
        # 1. Channel.CSV
        buf = io.StringIO()
        writer = csv.writer(buf)
        writer.writerow([
            "No.", "Channel Name", "Receive Frequency", "Transmit Frequency", "Channel Type",
            "Transmit Power", "Band Width", "CTCSS/DCS Decode", "CTCSS/DCS Encode", "Contact",
            "Contact Call Type", "Contact TG/DMR ID", "Radio ID", "Color Code", "Slot"
        ])
        for c in channels:
            no = c[0] if len(c) > 0 else "1"
            name = c[1] if len(c) > 1 else "CH"
            rx = c[2] if len(c) > 2 else "145.00000"
            tx = c[3] if len(c) > 3 else rx
            mode = c[4] if len(c) > 4 else "FM"
            pwr = c[5] if len(c) > 5 else "High"
            bw = c[6] if len(c) > 6 else "12.5K"
            ch_type = "D-Digital" if mode == "DMR" else "A-Analog"
            writer.writerow([no, name, rx, tx, ch_type, pwr, bw, "Off", "88.5", "TURKIYE", "Group Call", "286", "TA5THK", "1", "1"])
        zf.writestr("Channel.CSV", buf.getvalue().encode('utf-8-sig'))

        # 2. Zone.CSV
        buf = io.StringIO()
        writer = csv.writer(buf)
        writer.writerow(["No.", "Zone Name", "Zone Channel Member", "A Channel", "B Channel"])
        for i, z in enumerate(zones):
            z_no = str(i + 1)
            z_name = z.get('name', f"Zone {z_no}")
            members = "|".join(z.get('channels', []))
            writer.writerow([z_no, z_name, members, "", ""])
        zf.writestr("Zone.CSV", buf.getvalue().encode('utf-8-sig'))

        # 3. TalkGroups.CSV
        buf = io.StringIO()
        writer = csv.writer(buf)
        writer.writerow(["No.", "Radio ID", "Name", "Call Type", "Call Alert"])
        for i, tg in enumerate(talkgroups):
            tg_no = str(i + 1)
            tg_id = tg[0] if len(tg) > 0 else "286"
            tg_name = tg[1] if len(tg) > 1 else "TURKIYE"
            writer.writerow([tg_no, tg_id, tg_name, "Group Call", "None"])
        zf.writestr("TalkGroups.CSV", buf.getvalue().encode('utf-8-sig'))

    return output_path
