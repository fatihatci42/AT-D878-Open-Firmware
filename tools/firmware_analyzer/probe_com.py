import serial
import time
import sys

port = 'COM5'
print(f"[*] Opening {port} at different baud rates...")

bauds = [921600, 115200, 57600]

commands = [
    ("PROGRAM", b"PROGRAM"),
    ("fUfUPROGRAM", b"fUfUPROGRAM"),
    ("0x02+PROGRAM+0x06", b"\x02PROGRAM\x06"),
    ("0x02+D878+0x06", b"\x02D878\x06"),
    ("QX_06", b"QX\x06"),
    ("QX_06_END", b"QX\x06END\x00"),
    ("0x02_14bytes", b"\x02\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x06"),
    ("PRG", b"PRG\x06"),
    ("AT", b"AT\r\n"),
]

for baud in bauds:
    print(f"\n--- Testing Baud: {baud} ---")
    try:
        ser = serial.Serial(port, baudrate=baud, timeout=0.4, write_timeout=0.4)
    except Exception as e:
        print(f"[!] Cannot open {port}: {e}")
        break

    ser.reset_input_buffer()
    ser.reset_output_buffer()

    # Passive listen
    passive = ser.read(32)
    if passive:
        print(f"[+] Passive incoming data: {passive.hex()} ({passive})")

    for name, cmd in commands:
        try:
            ser.reset_input_buffer()
            ser.write(cmd)
            ser.flush()
            time.sleep(0.05)
            resp = ser.read(64)
            if resp:
                print(f"[+] MATCH! Cmd '{name}' -> Resp: {resp.hex()} ({resp})")
        except Exception as e:
            print(f"Error on cmd {name}: {e}")

    ser.close()
    time.sleep(0.1)

print("\n[*] Probe finished.")
