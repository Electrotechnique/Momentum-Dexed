"""Saves Momentum's screen as a PNG, over USB serial (the firmware answers 'S' with its frame buffer).

Needs pyserial and Pillow. Don't leave a serial monitor open on the port meanwhile.

    python tools/screenshot.py COM7 screen.png
"""
import sys
import time
import serial
from PIL import Image


def main(port, png):
    for attempt in range(3):  # Now and then a few bytes go missing over USB: try again
        data, w, h = grab(port)
        if len(data) == w * h * 2:
            break
        print(f"Short read ({len(data)} bytes), trying again")
    save(data, w, h, png)


def grab(port):
    with serial.Serial(port, 115200, timeout=3) as s:
        if hasattr(s, "set_buffer_size"):
            s.set_buffer_size(rx_size=262144)  # Windows: Teensy drops data if the PC falls behind
        time.sleep(0.2)
        s.reset_input_buffer()
        s.write(b"S")
        end = time.time() + 5
        line = b""
        while not line.startswith(b"SCREEN"):  # Skip CPUMonitor lines
            line = s.readline()
            if time.time() > end:
                sys.exit("No screenshot: is the firmware running?")
        w, h = (int(v) for v in line[6:].strip().split(b"x"))
        data = s.read(w * h * 2)
    return data, w, h


def save(data, w, h, png):
    if len(data) != w * h * 2:
        sys.exit(f"Short read: {len(data)} bytes")
    im = Image.new("RGB", (w, h))
    px = im.load()
    for y in range(h):
        for x in range(w):
            i = (y * w + x) * 2
            c = data[i] | (data[i + 1] << 8)
            px[x, y] = ((c >> 11) << 3, ((c >> 5) & 63) << 2, (c & 31) << 3)
    im.save(png)
    print(f"{png}: {w} x {h}")


if __name__ == "__main__":
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    main(*sys.argv[1:])
