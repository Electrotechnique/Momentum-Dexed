"""Converts a PNG into a C header for ILI9341_t3n's writeRect4BPP(): 16 grey levels on black.

For single colour logos (light on a transparent or black background). Transparent areas become
black, the empty border is trimmed. Needs Pillow (pip install pillow).

    python tools/png_to_4bpp.py images/dexed_logo.png src/DexedLogo.h DEXED_LOGO
"""
import sys
from PIL import Image


def main(png, header, name):
    im = Image.open(png).convert("RGBA")
    im = im.crop(im.getbbox())  # Trim the transparent border
    w, h = im.size
    rows = []
    for y in range(h):
        row = []
        for x in range(w):
            r, g, b, a = im.getpixel((x, y))
            grey = (r * 299 + g * 587 + b * 114) / 1000 * a / 255  # Over black
            row.append(round(grey * 15 / 255))
        if w % 2:
            row.append(0)  # Rows are padded to whole bytes
        rows.append(bytes((row[i] << 4) | row[i + 1] for i in range(0, len(row), 2)))
    palette = []
    for i in range(16):
        v = i * 17
        palette.append(((v >> 3) << 11) | ((v >> 2) << 5) | (v >> 3))  # RGB565

    with open(header, "w", newline="\r\n") as f:
        f.write(f"// {name}: generated from {png.replace(chr(92), '/')} by tools/png_to_4bpp.py, don't edit\n")
        f.write(f"// {w} x {h}, 4 bits per pixel (16 grey levels), for tft.writeRect4BPP()\n")
        f.write("#pragma once\n\n")
        f.write(f"#define {name}_W {w}\n#define {name}_H {h}\n\n")
        f.write(f"const uint16_t {name}_PALETTE[16] PROGMEM = {{\n  ")
        f.write(", ".join(f"0x{c:04X}" for c in palette))
        f.write("};\n\n")
        f.write(f"const uint8_t {name}_PIXELS[{len(rows) * len(rows[0])}] PROGMEM = {{\n")
        for r in rows:
            f.write("  " + ", ".join(f"0x{b:02X}" for b in r) + ",\n")
        f.write("};\n")
    print(f"{header}: {w} x {h}, {len(rows) * len(rows[0])} bytes")


if __name__ == "__main__":
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    main(*sys.argv[1:])
