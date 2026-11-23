import re

with open('878techmods.html', 'r', encoding='latin1', errors='ignore') as f:
    content = f.read()

def dump_topic(topic_id, max_len=3000):
    m = re.search(r'id=[\"\']?' + topic_id + r'[\"\']?[^>]*>(.*?)(?=<hr width=\"33%\"|<hr width=\"95%\"|$)', content, re.DOTALL | re.IGNORECASE)
    if m:
        text = re.sub(r'<[^>]+>', ' ', m.group(1))
        text = ' '.join(text.split())
        print(f"***** {topic_id} *****\n" + text[:max_len] + "\n\n")

for t in ['FlashMem', 'FullTestMode', 'TPTchirp', 'RadioMenuHint', 'popping', 'BatteryMeter']:
    dump_topic(t)
