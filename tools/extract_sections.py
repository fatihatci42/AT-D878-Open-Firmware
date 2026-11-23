import re

with open('878techmods.html', 'r', encoding='latin1', errors='ignore') as f:
    content = f.read()

topics = [
    'FlashMem', 'FullTestMode', 'Baseband', 'GenTech', 'Inside', 
    'FMBand', 'TPTchirp', 'RadioMenuHint', 'popping', 'FasterGPS', 
    'EnhancedMic', 'Backup', 'CDD', 'HWFault', 'PowerUpSequences',
    'SelectingBands', 'BatteryMeter', 'BatteryTips'
]

for t in topics:
    m = re.search(r'id=[\"\']?' + t + r'[\"\']?[^>]*>(.*?)(?=<a id=|<hr width=|$)', content, re.DOTALL | re.IGNORECASE)
    print(f"=== TOPIC: {t} ===")
    if m:
        text = re.sub(r'<[^>]+>', ' ', m.group(1))
        text = ' '.join(text.split())
        print(text[:800])
    else:
        print("Not found")
    print("-" * 50)
