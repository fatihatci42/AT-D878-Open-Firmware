#!/usr/bin/env python3
"""
AnyTone AT-D878 Serial / USB Protocol Analyzer & Inspector
Telsiz ile CPS arasındaki haberleşmeyi pasif analiz eder ve komut paketlerini ayrıştırır.
"""

import sys
import serial
import serial.tools.list_ports
import time
import argparse

CMD_PROGRAM_ENTRY = b"PROGRAM"
CMD_ACK = b"\x06"
CMD_NACK = b"\x15"

class AnyToneProtocolAnalyzer:
    def __init__(self, port, baudrate=921600, timeout=1.0):
        self.port = port
        self.baudrate = baudrate
        self.timeout = timeout
        self.ser = None

    def connect(self):
        print(f"[*] Seri porta bağlanılıyor: {self.port} ({self.baudrate} baud)...")
        self.ser = serial.Serial(self.port, self.baudrate, timeout=self.timeout)
        time.sleep(0.1)
        print("[+] Bağlantı başarılı.")

    def close(self):
        if self.ser and self.ser.is_open:
            self.ser.close()
            print("[*] Port kapatıldı.")

    def passive_sniff(self, duration_sec=10):
        """USB portundaki trafiği pasif olarak dinler ve hexdump üretir"""
        print(f"[*] Pasif dinleme modu başlatıldı ({duration_sec} saniye)...")
        start = time.time()
        buf = bytearray()
        try:
            while time.time() - start < duration_sec:
                data = self.ser.read(self.ser.in_waiting or 1)
                if data:
                    buf.extend(data)
                    hex_str = ' '.join(f'{b:02X}' for b in data)
                    print(f"[RECV {len(data)}B]: {hex_str}")
        except KeyboardInterrupt:
            print("[!] Kullanıcı tarafından durduruldu.")
        return buf

    def ping_program_mode(self):
        """Cihazın CPS programlama modunu test eder (Yazma yapmaz, sadece kimlik sorar)"""
        print("[*] Programlama modu sorgusu (PROGRAM) gönderiliyor...")
        self.ser.write(CMD_PROGRAM_ENTRY)
        self.ser.flush()
        resp = self.ser.read(64)
        if resp:
            print(f"[+] Yanıt alındı ({len(resp)} byte):")
            print("    HEX :", ' '.join(f'{b:02X}' for b in resp))
            print("    ASCII:", ''.join(chr(b) if 32 <= b <= 126 else '.' for b in resp))
        else:
            print("[-] Cihazdan yanıt alınamadı.")

def main():
    parser = argparse.ArgumentParser(description="AnyTone AT-D878 Protocol Analyzer")
    parser.add_argument("-p", "--port", help="Seri Port (Örn: COM3 veya /dev/ttyUSB0)")
    parser.add_argument("-b", "--baud", type=int, default=921600, help="Baud hızı (varsayılan: 921600)")
    parser.add_argument("-s", "--sniff", action="store_true", help="Pasif dinleme modu")
    parser.add_argument("-t", "--test-ident", action="store_true", help="Cihaz kimlik sorgusu gönder")
    args = parser.parse_args()

    if not args.port:
        print("[*] Mevcut COM portları listeleniyor:")
        ports = list(serial.tools.list_ports.comports())
        for p in ports:
            print(f"    - {p.device}: {p.description} [{p.hwid}]")
        print("\nKullanım: python anytone_protocol.py -p <PORT> [--sniff | --test-ident]")
        return

    analyzer = AnyToneProtocolAnalyzer(args.port, args.baud)
    try:
        analyzer.connect()
        if args.test_ident:
            analyzer.ping_program_mode()
        elif args.sniff:
            analyzer.passive_sniff(30)
    finally:
        analyzer.close()

if __name__ == "__main__":
    main()
