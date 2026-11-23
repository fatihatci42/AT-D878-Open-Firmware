import re
import json

with open('webcps_project.html', 'r', encoding='utf-8') as f:
    text = f.read()

# Check for embedded json or scripts
print("Checking for embedded project json...")
for m in re.finditer(r'<script[^>]*>(.*?)</script>', text, re.DOTALL):
    sc = m.group(1)
    if 'project' in sc.lower() and ('channels' in sc.lower() or 'zones' in sc.lower() or 'talkgroups' in sc.lower()):
        print("Found relevant script block! Length:", len(sc))
        # Look for JSON assignments
        for line in sc.split('\n'):
            if any(k in line for k in ['Project', 'channels', 'zones', 'rdt', 'export']):
                print("  Line:", line[:120])

# Look for table data
tables = re.findall(r'<table[^>]*>(.*?)</table>', text, re.DOTALL)
print(f"Total tables: {len(tables)}")

# Look for export links
for m in re.finditer(r'(https?://[^\s"\'<>]+|/api/[^\s"\'<>]+)', text):
    u = m.group(1)
    if any(k in u for k in ['export', 'download', 'rdt', 'csv', 'projects']):
        print("Found URL:", u)
