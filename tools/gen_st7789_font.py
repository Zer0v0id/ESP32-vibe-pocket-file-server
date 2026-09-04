#!/usr/bin/env python3
"""Rasterize a crisp 1bpp 10x18 ASCII font for the T-Embed ST7789 UI."""
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

CELL_W = 12
CELL_H = 20
FIRST = 32
LAST = 126
FONT_PATH = "/System/Library/Fonts/Supplemental/Courier New Bold.ttf"
FONT_SIZE = 16


def load_font():
    return ImageFont.truetype(FONT_PATH, size=FONT_SIZE)


def main():
    font = load_font()
    glyphs = []
    preview = Image.new("L", (CELL_W * 16, CELL_H * 6), 0)
    for i, code in enumerate(range(FIRST, LAST + 1)):
        ch = chr(code)
        img = Image.new("L", (CELL_W, CELL_H), 0)
        d = ImageDraw.Draw(img)
        bbox = font.getbbox(ch)
        gw = bbox[2] - bbox[0]
        gh = bbox[3] - bbox[1]
        x0 = max(0, (CELL_W - gw) // 2 - bbox[0])
        y0 = max(0, 2 - bbox[1])
        d.text((x0, y0), ch, font=font, fill=255)
        # High threshold: solid pixels only, no gray fringe.
        packed = []
        for y in range(CELL_H):
            bits = 0
            for x in range(CELL_W):
                if img.getpixel((x, y)) >= 140:
                    bits |= 1 << x
                    img.putpixel((x, y), 255)
                else:
                    img.putpixel((x, y), 0)
            packed.append(bits & 0xFF)
            packed.append((bits >> 8) & 0xFF)
        glyphs.append(packed)
        preview.paste(img, ((i % 16) * CELL_W, (i // 16) * CELL_H))

    out_h = Path(__file__).resolve().parents[1] / "main" / "display_font_ui.h"
    bytes_per = CELL_H * 2
    lines = [
        "/* Auto-generated 1bpp %dx%d font (ASCII 32-126). */" % (CELL_W, CELL_H),
        "/* Source: %s @ %d, thresholded. */" % (FONT_PATH, FONT_SIZE),
        "#pragma once",
        "#include <stdint.h>",
        "",
        "#define DISPLAY_FONT_W %d" % CELL_W,
        "#define DISPLAY_FONT_H %d" % CELL_H,
        "#define DISPLAY_FONT_FIRST 32",
        "#define DISPLAY_FONT_COUNT %d" % (LAST - FIRST + 1),
        "",
        "static const uint8_t display_font_bits[%d][%d] = {" % (LAST - FIRST + 1, bytes_per),
    ]
    for gi, g in enumerate(glyphs):
        ch = FIRST + gi
        comment = chr(ch) if 32 < ch < 127 and chr(ch) not in "\\/*" else hex(ch)
        hexes = ", ".join("0x%02X" % b for b in g)
        lines.append("    { %s }, /* %s */" % (hexes, comment))
    lines.append("};")
    lines.append("")
    out_h.write_text("\n".join(lines))
    preview_path = Path(__file__).resolve().parent / "font_preview.png"
    preview.resize((preview.width * 4, preview.height * 4), Image.NEAREST).save(preview_path)
    print("wrote", out_h)
    print("preview", preview_path)


if __name__ == "__main__":
    main()
