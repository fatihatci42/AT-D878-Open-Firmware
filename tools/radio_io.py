#!/usr/bin/env python3
"""
radio_io.py — Cross-platform D878UV PC-link bridge.

Tersine mühendislikle doğrulanmış PC/USB protokolünü
(reverse-engineering/docs/pc_link.md) Windows dahil tüm platformlarda
kullanılabilir hale getirir. RE alt klasöründeki `radio.py` aracı
`fcntl`/`termios` kullandığından yalnızca Linux'ta çalışır; bu modül aynı
protokolün saf-pyserial implementasyonudur ve AT-D878 Studio'dan
`comm` katmanı üzerinden çağrılabilir.

Protokol (stok firmware V4.00, g_2000D5A8 command buffer):
  PROGRAM         -> QX 06                      (read/CPS session açar)
  0x02            -> model + region + V101     (kimlik)
  R + BE32 + len  -> W addr len data sum8 06   (SPI flash oku, len 1..255)
  END             -> 06                         (reboot, session kapat)

Güvenlik: Bu modül YALNIZCA okur. W / H / 0x01 / 0xA4 / Y komutları
kasıtlı olarak implement edilmedi (flash yazar, Y vericiyi tuşlar).

Kullanım (CLI):
  python tools/radio_io.py --port COM3 info
  python tools/radio_io.py --port COM3 dump backup.bin
  python tools/radio_io.py --port COM3 read 0x2FA0000 0x100

Kullanım (kütüphane):
  from tools.radio_io import RadioLink
  link = RadioLink("COM3")
  link.open_session()
  print(link.identity())
  data = link.read_flash(0x02500000, 256)   # radio_settings mirror
  link.end_session()
"""

from __future__ import annotations

import argparse
import struct
import sys
import time
from pathlib import Path

try:
    import serial
    import serial.tools.list_ports
except ImportError:  # pragma: no cover
    serial = None


GD32_VID = 0x28E9          # GD32 Virtual ComPort USB VID
GD32_PID = 0x018A
ACK = 0x06
READ_MAX = 255             # R komutu en fazla 255 bayt (1 KB reply buffer)
NAND_SIZE = 0x8000000      # 128 MB (1 Gbit) — üstü adresler yok sayılır

# Stok codeplug bellek haritası (reald / anytone-flash-tools)
FLASH_MAP = [
    (0x02500000, "Codeplug: genel ayarlar (radio_settings)"),
    (0x0FC0800,  "Codeplug: VFO A/B kanal kayıtları (4000/4001)"),
    (0x02FA0000, "Factory block A (kalibrasyon, RF variant tag)"),
    (0x02FC0000, "Factory block B (yedek bank)"),
    (0x04000000, "DMR ID / dijital kontaklar"),
    (0x0200000,  "GBK16 font (icon pack)"),
]


class RadioLinkError(Exception):
    """PC-link protokol hatası."""


class RadioLink:
    """AnyTone D878 PC-link oturumu. Okuma-yönlü, güvenli sarmalayıcı."""

    def __init__(self, port: str, timeout: float = 1.5):
        if serial is None:
            raise RadioLinkError("pyserial kurulu değil: pip install pyserial")
        self.port_name = port
        self.timeout = timeout
        self.ser = None
        self.in_session = False
        self.model = ""
        self.hw_version = ""

    # ------------------------------------------------------------- bağlantı
    def open(self) -> None:
        self.ser = serial.Serial(
            port=self.port_name,
            baudrate=115200,          # USB CDC: baud yok sayılır
            timeout=self.timeout,
            write_timeout=self.timeout,
        )
        self.ser.reset_input_buffer()

    def close(self) -> None:
        if self.ser and self.ser.is_open:
            self.ser.close()

    # ------------------------------------------------------- düşük seviye
    def _xfer(self, request: bytes, expect: int) -> bytes:
        self.ser.reset_input_buffer()
        self.ser.write(request)
        self.ser.flush()
        reply = self.ser.read(expect)
        if not reply:
            raise RadioLinkError(
                f"Radyo yanıt vermedi (istek: {request[:8]!r})"
            )
        return reply

    # ---------------------------------------------------------- oturum
    def open_session(self) -> None:
        """PROGRAM el sıkışması -> QX 06 (read/CPS session)."""
        reply = self._xfer(b"PROGRAM", 16)
        if b"QX" not in reply:
            raise RadioLinkError(
                f"PROGRAM el sıkışması başarısız: {reply.hex()}"
            )
        self.in_session = True

    def end_session(self) -> None:
        """END -> radyo reboot olur, normal moda döner."""
        if not self.in_session:
            return
        try:
            self.ser.write(b"END")
            self.ser.flush()
        finally:
            self.in_session = False

    # ---------------------------------------------------------- kimlik
    def identity(self) -> dict:
        """0x02 komutu: model + donanım/bölge + V101."""
        reply = self._xfer(b"\x02", 32)
        clean = reply.replace(b"\x06", b"")
        fields = [
            p.decode("ascii", errors="ignore")
            for p in clean.split(b"\x00") if p
        ]
        self.model = next((f for f in fields if "D878" in f.upper()), fields[0] if fields else "?")
        self.hw_version = fields[1] if len(fields) > 1 else "?"
        return {
            "raw": reply.hex(),
            "model": self.model,
            "hw": self.hw_version,
            "fields": fields,
        }

    # --------------------------------------------------------- SPI flash
    def read_flash(self, addr: int, length: int, progress=None) -> bytes:
        """SPI NAND mantıksal okuma (R komutu, A/B-bank seçimli).

        Dikkat: R ham NAND değil CPS mantıksal görünümüdür
        (pc_factory_select_bank, 128 KB blok A/B seçimi).
        """
        if not self.in_session:
            raise RadioLinkError("Önce open_session() çağırın")
        if addr < 0 or addr + length > NAND_SIZE:
            raise RadioLinkError(
                f"Adres aralığı SPI NAND dışında: 0x{addr:X}+0x{length:X}"
            )
        if length <= 0:
            return b""

        out = bytearray()
        total = length
        while length > 0:
            chunk = min(READ_MAX, length)
            req = b"R" + struct.pack(">I", addr) + bytes([chunk])
            reply = self._xfer(req, chunk + 8)
            if reply[0] != 0x57:  # 'W'
                raise RadioLinkError(
                    f"READ yanıtı bozuk (0x{reply[0]:02X} != 0x57)"
                )
            # W addr(4) len(1) data.. sum8(1) 06(1)
            r_addr = struct.unpack(">I", reply[1:5])[0]
            r_len = reply[5]
            data = reply[6:6 + r_len]
            if r_addr != addr or r_len != chunk:
                raise RadioLinkError(
                    f"READ adres/uzunluk uyumsuz: istek 0x{addr:X}+{chunk}, "
                    f"yanıt 0x{r_addr:X}+{r_len}"
                )
            # 8-bit toplam checksum (1..n, addr+len hariç data dahil değil —
            # yanıt baytları 1..n-2 üzerinden; stok sum8 data'yı kapsar)
            out += data
            addr += chunk
            length -= chunk
            if progress:
                done = total - length
                progress(int(done * 100 / total), f"0x{r_addr:X}")
        return bytes(out)

    def dump(self, out_path, start: int = 0, end: int = NAND_SIZE,
             resume: bool = True, progress=None) -> Path:
        """SPI NAND mantıksal dump (~150 KB/s @ 255'lik bloklar)."""
        out = Path(out_path)
        out.parent.mkdir(parents=True, exist_ok=True)
        if resume and out.exists():
            start += out.stat().st_size
            if start >= end:
                print(f"Dump zaten tamam: {out} ({out.stat().st_size} bayt)")
                return out
        count = end - start
        if count <= 0:
            return out

        def cb(pct, at):
            if progress:
                progress(pct, at)

        data = self.read_flash(start, count, progress=cb)
        mode = "ab" if (resume and out.exists()) else "wb"
        with open(out, mode) as fh:
            fh.write(data)
        return out

    # -------------------------------------------------- yardımcılar
    def read_radio_settings(self) -> bytes:
        """Genel ayarlar bloğu (codeplug 0x02500000, 256 B radio_settings)."""
        return self.read_flash(0x02500000, 256)

    def read_factory_block_head(self) -> bytes:
        """Factory block başı (RF variant tag ofset 0x120)."""
        return self.read_flash(0x02FA0000, 0x200)

    def read_vfo_records(self) -> bytes:
        """VFO A/B kanal kayıtları (0xFC0800, 2 × 0x80)."""
        return self.read_flash(0x00FC0800, 0x100)


# ----------------------------------------------------------------- CLI
def _find_gd32_port() -> str | None:
    for p in serial.tools.list_ports.comports():
        if p.vid == GD32_VID and p.pid == GD32_PID:
            return p.device
    return None


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(
        description="D878UV PC-link okuma aracı (reverse-engineering köprüsü)"
    )
    ap.add_argument("--port", default=None, help="Seri port (varsayılan: GD32 VID autodetect)")
    ap.add_argument("--timeout", type=float, default=1.5)
    sub = ap.add_subparsers(dest="cmd", required=True)

    sub.add_parser("info", help="Kimlik sorgula, oturumu kapat")
    d = sub.add_parser("dump", help="SPI NAND mantıksal dump")
    d.add_argument("out", help="Çıktı dosyası")
    d.add_argument("--start", default="0x0", help="Başlangıç adresi (hex)")
    d.add_argument("--end", default=f"0x{NAND_SIZE:X}", help="Bitiş adresi (hex)")
    r = sub.add_parser("read", help="Adres aralığını hexdump et")
    r.add_argument("addr", help="Başlangıç (hex)")
    r.add_argument("length", help="Uzunluk (hex veya ondalık)")
    sub.add_parser("map", help="Bellek haritasını göster (radyoya bağlanmaz)")

    args = ap.parse_args(argv)

    if args.cmd == "map":
        for addr, desc in FLASH_MAP:
            print(f"0x{addr:08X}  {desc}")
        return 0

    port = args.port or _find_gd32_port()
    if not port:
        print("Hata: GD32 Virtual ComPort bulunamadı. --port ile belirtin.")
        return 1

    link = RadioLink(port, timeout=args.timeout)
    try:
        link.open()
        link.open_session()
        if args.cmd == "info":
            ident = link.identity()
            print(f"Model    : {ident['model']}")
            print(f"Donanım  : {ident['hw']}")
            print(f"Ham yanıt: {ident['raw']}")
        elif args.cmd == "dump":
            start, end = int(args.start, 0), int(args.end, 0)
            print(f"Dump: 0x{start:X}..0x{end:X} -> {args.out} (~150 KB/s)")
            t0 = time.time()
            link.dump(args.out, start, end, progress=lambda p, a: print(f"\r{p:3d}% @ 0x{a:X}", end=""))
            print(f"\nTamam: {time.time() - t0:.0f} s")
        elif args.cmd == "read":
            addr = int(args.addr, 0)
            length = int(args.length, 0)
            data = link.read_flash(addr, length)
            for off in range(0, len(data), 16):
                row = data[off:off + 16]
                print(f"{addr + off:08X}  {row.hex(' ')}")
        link.end_session()
    except RadioLinkError as exc:
        print(f"Hata: {exc}", file=sys.stderr)
        return 1
    finally:
        link.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
