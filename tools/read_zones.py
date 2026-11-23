import zipfile
import csv
import io

zip_path = r'C:\Users\Fatih ATCI\Desktop\Anytone\webcps_export.zip'
with zipfile.ZipFile(zip_path) as z:
    with z.open('Zone.CSV') as f:
        reader = csv.reader(io.TextIOWrapper(f, encoding='utf-8', errors='ignore'))
        rows = list(reader)
        print(f"Total zones: {len(rows)-1}")
        for r in rows[1:]:
            channels = [ch for ch in r[2].split('|') if ch]
            print(f"  Zone {r[0]}: {r[1]:25s} (Channels: {len(channels)})")
