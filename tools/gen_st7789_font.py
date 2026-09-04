#!/usr/bin/env python3
"""Rasterize a 2bpp 12x20 ASCII font for the T-Embed ST7789 UI."""
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

CELL_W = 12
CELL_H = 20
FIRST = 32
LAST = 126
FONT_PATH = "/System/Library/Fonts/Menlo.ttc"
FONT_INDEX = 1
FONT_SIZE = 15
ROW_BYTES = (CELL_W * 2 + 7) // 8


def load_font():
    return ImageFont.truetype(FONT_PATH, size=FONT_SIZE, index=FONT_INDEX)


def quantize(p):
    # Keep coverage punchy without 1bpp stairsteps.
    t = (p / 255.0) ** 0.55
    return max(0, min(3, int(round(t * 3))))


def pack_row(levels):
    bits = 0
    for x, level in enumerate(levels):
        bits |= (level & 3) << (x * 2)
    return [(bits >> (8 * i)) & 0xFF for i in range(ROW_BYTES)]


def main():
    font = load_font()
    glyphs = []
    preview = Image.new("L", (CELL_W * 16, CELL_H * 6), 0)
    for i, code in enumerate(range(FIRST, LAST + 1)):
        ch = chr(code)
        img = Image.new("L", (CELL_W, CELL_H), 0)
        d = ImageDraw.Draw(img)
        d.text((1, 1), ch, font=font, fill=255)
        packed = []
        for y in range(CELL_H):
            levels = []
            for x in range(CELL_W):
                level = quantize(img.getpixel((x, y)))
                levels.append(level)
                img.putpixel((x, y), level * 85)
            packed.extend(pack_row(levels))
        glyphs.append(packed)
        preview.paste(img, ((i % 16) * CELL_W, (i // 16) * CELL_H))

    out_h = Path(__file__).resolve().parents[1] / "main" / "display_font_ui.h"
    bytes_per = CELL_H * ROW_BYTES
    lines = [
        "/* Auto-generated 2bpp %dx%d font (ASCII 32-126). */" % (CELL_W, CELL_H),
        "/* Source: %s index %d @ %d. bit0-1 = leftmost pixel. */"
        % (FONT_PATH, FONT_INDEX, FONT_SIZE),
        "#pragma once",
        "#include <stdint.h>",
        "",
        "#define DISPLAY_FONT_W %d" % CELL_W,
        "#define DISPLAY_FONT_H %d" % CELL_H,
        "#define DISPLAY_FONT_BPP 2",
        "#define DISPLAY_FONT_ROW_BYTES %d" % ROW_BYTES,
        "#define DISPLAY_FONT_FIRST 32",
        "#define DISPLAY_FONT_COUNT %d" % (LAST - FIRST + 1),
        "",
        "static const uint8_t display_font_bits[%d][%d] = {"
        % (LAST - FIRST + 1, bytes_per),
    ]
    for gi, g in enumerate(glyphs):
        ch = FIRST + gi
        comment = chr(ch) if 32 < ch < 127 and chr(ch) not in "\\/*" else hex(ch)
        hexes = ", ".join("0x%02X" % b for b in g)
        lines.append("    { %s }, /* %s */" % (hexes, comment))
    lines.append("};")
    lines.append("")
    out_h.write_text("\n".join(lines))
    preview_path = Path("/tmp/font_preview.png")
    preview.resize((preview.width * 4, preview.height * 4), Image.NEAREST).save(preview_path)
    print("wrote", out_h)
    print("preview", preview_path)


if __name__ == "__main__":
    main()
