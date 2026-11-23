import urllib.request
import re

url = "https://cps.aes.app/projects/65c5cbc9-8182-4243-969b-a379f37a6beb"
req = urllib.request.Request(url, headers={'User-Agent': 'Mozilla/5.0'})
try:
    with urllib.request.urlopen(req) as resp:
        html = resp.read().decode('utf-8', errors='ignore')
    
    with open('webcps_project.html', 'w', encoding='utf-8') as f:
        f.write(html)
    print(f"Saved webcps_project.html, length: {len(html)}")
    
    links = re.findall(r'href=["\']([^"\']+)["\']', html)
    for l in set(links):
        if any(k in l.lower() for k in ['download', 'export', 'rdt', 'csv', 'json', 'channel', 'zone', 'talkgroup', 'project', 'api']):
            print("Link:", l)
            
    # Search for project title, channel count, zone count, radio model
    print("\nPage text snippets:")
    for m in re.finditer(r'<h[1-4][^>]*>(.*?)</h[1-4]>', html, re.DOTALL):
        print("Header:", re.sub(r'<[^>]+>', '', m.group(1)).strip())
except Exception as e:
    print("Error:", e)
