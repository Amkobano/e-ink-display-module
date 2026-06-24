"""
render_dua_images.py — Pre-render each dua as an 800×480 1-bit BMP.

Uses HarfBuzz (uharfbuzz) for text shaping with full GPOS mark positioning,
so tashkeel marks (kasra, fatha, damma, sukun …) are placed correctly relative
to Arabic letter dots instead of overlapping them.
FreeType (freetype-py) rasterises each shaped glyph at its HarfBuzz position.

Usage:
    pip install Pillow uharfbuzz freetype-py
    python render_dua_images.py

Input:  ../data/dua-dhikr.json  (arabic field, with tashkeel)
Output: ../esp32-firmware/data/dua_000.bmp … dua_NNN.bmp

After running, flash the filesystem:
    cd ../esp32-firmware && pio run -t uploadfs
"""

import json
import sys
from pathlib import Path
from typing import List, Tuple

try:
    from PIL import Image
except ImportError:
    print("Missing dependency: pip install Pillow")
    sys.exit(1)

try:
    import uharfbuzz as hb
except ImportError:
    print("Missing dependency: pip install uharfbuzz")
    sys.exit(1)

try:
    import freetype
except ImportError:
    print("Missing dependency: pip install freetype-py")
    sys.exit(1)

# ── Paths ──────────────────────────────────────────────────────────────────────
ROOT       = Path(__file__).parent.parent
INPUT_PATH = ROOT / "data" / "dua-dhikr.json"
OUTPUT_DIR = ROOT / "esp32-firmware" / "data"
FONT_PATH  = Path(__file__).parent / "fonts" / "NotoNaskhArabic-Bold.ttf"

# ── Display constants ──────────────────────────────────────────────────────────
DISPLAY_W = 800
DISPLAY_H = 480
MARGIN_X  = 40    # horizontal margin on each side
MARGIN_Y  = 30    # vertical margin (extra room for tashkeel at edges)
USABLE_W  = DISPLAY_W - 2 * MARGIN_X
USABLE_H  = DISPLAY_H - 2 * MARGIN_Y
LINE_GAP  = 20    # extra px between lines (≈1.5× spacing for tashkeel room)

FONT_SIZE_MAX  = 120
FONT_SIZE_MIN  = 14
FONT_SIZE_STEP = 4

# Load HarfBuzz blob/face once — reading the font file is the slow part.
_hb_blob = hb.Blob.from_file_path(str(FONT_PATH))
_hb_face = hb.Face(_hb_blob)


# ── Font helpers ───────────────────────────────────────────────────────────────

def _hb_font(size: int) -> hb.Font:
    """HarfBuzz font scaled so positions are in 26.6 FP (>>6 gives pixels)."""
    f = hb.Font(_hb_face)
    f.scale = (size * 64, size * 64)
    hb.ot_font_set_funcs(f)
    return f


def _ft_face(size: int) -> freetype.Face:
    """FreeType face at size pt, 72 DPI (1 pt = 1 px)."""
    f = freetype.Face(str(FONT_PATH))
    f.set_char_size(size * 64)
    return f


# ── Shaping ────────────────────────────────────────────────────────────────────

def _shape(text: str, font: hb.Font):
    """
    Shape Arabic text with HarfBuzz.

    Returns glyphs in VISUAL LEFT-TO-RIGHT order (for RTL Arabic, the logically
    last character is leftmost and appears first in the list).  Base glyphs have
    positive x_advance; marks (kasra, fatha, …) have x_advance=0 and
    x_offset/y_offset set by GPOS so they don't collide with letter dots.
    """
    buf = hb.Buffer()
    buf.add_str(text)
    buf.guess_segment_properties()   # detects RTL direction + Arabic script
    hb.shape(font, buf)
    return buf.glyph_infos, buf.glyph_positions


def _width_px(text: str, font: hb.Font) -> int:
    """Total advance width of shaped text in pixels."""
    _, positions = _shape(text, font)
    return sum(p.x_advance for p in positions) >> 6


# ── Text wrapping ──────────────────────────────────────────────────────────────

def _wrap(text: str, font: hb.Font) -> List[str]:
    """
    Wrap Arabic text into lines that fit USABLE_W.

    Words are accumulated in logical order; HarfBuzz handles the RTL reordering
    per line during rendering.  No post-reversal needed here.
    """
    words = text.split()
    lines: List[str] = []
    current = ""
    for word in words:
        candidate = (current + " " + word).strip() if current else word
        if _width_px(candidate, font) <= USABLE_W:
            current = candidate
        else:
            if current:
                lines.append(current)
            current = word
    if current:
        lines.append(current)
    return lines


def _line_height(size: int) -> int:
    """Line height in pixels: font ascender + |descender| + LINE_GAP."""
    ft = _ft_face(size)
    return ((ft.size.ascender - ft.size.descender) >> 6) + LINE_GAP


# ── Fit search ─────────────────────────────────────────────────────────────────

def find_best_fit(text: str) -> Tuple[List[str], int]:
    """Return (lines, font_size) for the largest size that fits the display."""
    for size in range(FONT_SIZE_MAX, FONT_SIZE_MIN - 1, -FONT_SIZE_STEP):
        font  = _hb_font(size)
        lines = _wrap(text, font)
        lh    = _line_height(size)
        total_h = len(lines) * lh - LINE_GAP
        if total_h <= USABLE_H:
            return lines, size
    font = _hb_font(FONT_SIZE_MIN)
    return _wrap(text, font), FONT_SIZE_MIN


# ── Rendering ──────────────────────────────────────────────────────────────────

def render_line(img: Image.Image, text: str,
                font: hb.Font, ft: freetype.Face,
                cx: int, baseline_y: int) -> None:
    """
    Draw one line of shaped Arabic text, horizontally centred at cx.

    Each glyph (including marks) is placed at the position computed by
    HarfBuzz GPOS, so kasra appears below the letter dot, fatha above it, etc.

    Coordinate conventions:
      cursor_x / baseline_y  — screen coords, y increases downward
      pos.x_offset / y_offset — HarfBuzz 26.6 FP; positive y = UP (font coords)
      bitmap_top              — FreeType pixels above the baseline (positive = up)
    """
    infos, positions = _shape(text, font)

    # Centre the line: total advance width = sum of base advances (marks are 0)
    total_w = sum(p.x_advance for p in positions) >> 6
    cursor_x = cx - total_w // 2

    pixels = img.load()

    for info, pos in zip(infos, positions):
        try:
            ft.load_glyph(info.codepoint,
                          freetype.FT_LOAD_RENDER | freetype.FT_LOAD_TARGET_NORMAL)
        except Exception:
            cursor_x += pos.x_advance >> 6
            continue

        bm = ft.glyph.bitmap
        if bm.width > 0 and bm.rows > 0:
            # x_offset: horizontal shift from cursor (centres mark over base)
            # y_offset: vertical shift; negative = downward on screen
            ox = pos.x_offset >> 6
            oy = pos.y_offset >> 6   # negative for below-baseline marks (kasra …)

            # Top-left corner of this glyph's bitmap in screen coords:
            #   gx = cursor + GPOS x-offset + FT bearing
            #   gy = baseline - GPOS y-offset (sign flip: font↑ = screen↓)
            #              - FT bearing (bitmap_top pixels above baseline)
            gx = cursor_x + ox + ft.glyph.bitmap_left
            gy = baseline_y - oy - ft.glyph.bitmap_top

            for row in range(bm.rows):
                for col in range(bm.width):
                    if bm.buffer[row * bm.pitch + col] > 127:
                        px, py = gx + col, gy + row
                        if 0 <= px < img.width and 0 <= py < img.height:
                            pixels[px, py] = 0   # black on white

        cursor_x += pos.x_advance >> 6


def render_dua(text: str, output_path: Path) -> int:
    """Render Arabic text onto 800×480 1-bit BMP. Returns font size used."""
    lines, size = find_best_fit(text)
    font     = _hb_font(size)
    ft       = _ft_face(size)
    lh       = _line_height(size)
    ascender = ft.size.ascender >> 6

    total_h = len(lines) * lh - LINE_GAP
    start_y = MARGIN_Y + (USABLE_H - total_h) // 2

    img = Image.new("1", (DISPLAY_W, DISPLAY_H), 1)   # white background

    for i, line in enumerate(lines):
        baseline_y = start_y + i * lh + ascender
        render_line(img, line, font, ft, DISPLAY_W // 2, baseline_y)

    img.save(str(output_path))
    return size


# ── Entry point ────────────────────────────────────────────────────────────────

def main() -> None:
    print(f"Reading: {INPUT_PATH}")
    with open(INPUT_PATH, encoding="utf-8") as f:
        entries = json.load(f)

    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    print(f"Rendering {len(entries)} dua images → {OUTPUT_DIR}")
    print()

    for idx, entry in enumerate(entries):
        arabic = entry.get("arabic", "")
        if not arabic:
            print(f"  [{idx:03d}] SKIP — no arabic text")
            continue

        output_path = OUTPUT_DIR / f"dua_{idx:03d}.bmp"
        size = render_dua(arabic, output_path)
        kb   = output_path.stat().st_size / 1024
        print(f"  [{idx:03d}] {size:3d}pt  {kb:.1f} KB  — {entry.get('title', '')[:50]}")

    bmp_files = list(OUTPUT_DIR.glob("dua_*.bmp"))
    total_kb  = sum(f.stat().st_size for f in bmp_files) / 1024
    print()
    print(f"Done. {len(bmp_files)} images, {total_kb:.0f} KB ({total_kb/1024:.1f} MB)")
    print()
    print("Next step: flash the filesystem to the ESP32:")
    print("  cd ../esp32-firmware && pio run -t uploadfs")


if __name__ == "__main__":
    main()
