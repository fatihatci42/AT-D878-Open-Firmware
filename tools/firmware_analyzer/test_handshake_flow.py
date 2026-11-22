import serial
import time

port = 'COM5'
print(f"[*] Opening {port} at 921600...")
ser = serial.Serial(port, baudrate=921600, timeout=1.0)

# Step 1: Handshake with PROGRAM
ser.reset_input_buffer()
ser.reset_output_buffer()
print("-> Sending PROGRAM")
ser.write(b"PROGRAM")
ser.flush()
resp = ser.read(32)
print(f"<- Received: {resp.hex()} ({resp})")

# Step 2: What commands can we send now?
# In AnyTone protocol:
# '2' or 'R' or 'IDENT' or 'D878' or 0x02 framing
test_cmds = [
    ("b'\\x02'", b"\x02"),
    ("b'D878'", b"D878"),
    ("b'\\x02D878\\x06'", b"\x02D878\x06"),
    ("b'\\x02ID\\x06'", b"\x02ID\x06"),
    ("b'\\x02R\\x00\\x00\\x00\\x00\\x10\\x06'", b"\x02R\x00\x00\x00\x00\x10\x06"), # Read 16 bytes from 0x00
    ("b'\\x02READ'", b"\x02READ"),
    ("b'V200'", b"V200"),
    ("b'QX'", b"QX"),
    ("b'\\x06'", b"\x06"),
]

for name, cmd in test_cmds:
    print(f"\n-> Testing: {name}")
    ser.reset_input_buffer()
    ser.write(cmd)
    ser.flush()
    time.sleep(0.1)
    r = ser.read(64)
    print(f"<- Resp: {r.hex()} ({r})")

ser.close()
print("\n[*] Done.")
