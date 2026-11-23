import zipfile
import csv
import io
import json
import os

zip_path = r'C:\Users\Fatih ATCI\Desktop\Anytone\webcps_export.zip'
codeplug = {
    'name': 'AT-D878UVII TA1UAT / TA5THK Türkiye Röle & DMR Kodplug',
    'version': '2026-11-08',
    'source': 'https://cps.aes.app/projects/65c5cbc9-8182-4243-969b-a379f37a6beb',
    'channels': [],
    'zones': [],
    'talkgroups': [],
    'fm': []
}

with zipfile.ZipFile(zip_path) as z:
    # 1. Channels
    with z.open('Channel.CSV') as f:
        reader = csv.DictReader(io.TextIOWrapper(f, encoding='utf-8', errors='ignore'))
        for r in reader:
            mode = 'FM' if r.get('Channel Type', '') == 'A-Analog' else 'DMR'
            if mode == 'DMR':
                info = f"CC:{r.get('Color Code', '1')} | TS{r.get('Slot', '1')} | {r.get('Contact', '')}"
            else:
                enc = r.get('CTCSS/DCS Encode', 'Off')
                info = f"CTCSS: {enc}" if enc and enc != 'Off' else "CSQ"
            
            codeplug['channels'].append([
                r.get('No.', ''),
                r.get('Channel Name', ''),
                r.get('Receive Frequency', ''),
                r.get('Transmit Frequency', ''),
                mode,
                r.get('Transmit Power', 'High'),
                r.get('Band Width', '12.5K'),
                info
            ])

    # 2. Zones
    with z.open('Zone.CSV') as f:
        reader = csv.DictReader(io.TextIOWrapper(f, encoding='utf-8', errors='ignore'))
        for r in reader:
            ch_list = [c for c in r.get('Zone Channel Member', '').split('|') if c]
            codeplug['zones'].append({
                'no': r.get('No.', ''),
                'name': r.get('Zone Name', ''),
                'channels': ch_list
            })

    # 3. TalkGroups
    with z.open('TalkGroups.CSV') as f:
        reader = csv.DictReader(io.TextIOWrapper(f, encoding='utf-8', errors='ignore'))
        for r in reader:
            codeplug['talkgroups'].append([
                r.get('Radio ID', ''),
                r.get('Name', ''),
                r.get('Name', ''),
                'TURKIYE',
                'TR',
                f"{r.get('Call Type', 'Group Call')} (TG {r.get('Radio ID', '')})"
            ])

    # 4. FM
    with z.open('FM.CSV') as f:
        reader = csv.DictReader(io.TextIOWrapper(f, encoding='utf-8', errors='ignore'))
        for r in reader:
            codeplug['fm'].append({
                'no': r.get('No.', ''),
                'freq_mhz': r.get('Frequency[MHz]', '')
            })

out_dir = r'desktop\at_d878_studio\data'
os.makedirs(out_dir, exist_ok=True)
out_path = os.path.join(out_dir, 'turkey_codeplug_template.json')

with open(out_path, 'w', encoding='utf-8') as f:
    json.dump(codeplug, f, ensure_ascii=False, indent=2)

print(f"Başarıyla kaydedildi: {out_path}")
print(f"İçerik: {len(codeplug['channels'])} Kanal, {len(codeplug['zones'])} Zone, {len(codeplug['talkgroups'])} TalkGroup, {len(codeplug['fm'])} FM İstasyonu")
