"""
render_dua_images.py — Pre-render each dua as an 800×480 1-bit BMP.

Each image has two stacked sections:
  • Top    — Arabic text, shaped with HarfBuzz (full GPOS mark positioning so
             tashkeel sits correctly relative to letter dots), rasterised glyph
             by glyph with FreeType.
  • Bottom — English translation, drawn with Pillow using Noto Sans.
A thin horizontal rule separates the two.

Both sections are auto-sized together (largest Arabic that fits its height
budget, then the largest translation that fits the remaining space) so every
dua stays readable regardless of length.

Usage:
    pip install Pillow uharfbuzz freetype-py
    python render_dua_images.py

Input:  ../data/dua-dhikr.json  (arabic + translation fields)
Output: ../esp32-firmware/data/dua_000.bmp … dua_NNN.bmp

After running, flash the filesystem:
    cd ../esp32-firmware && pio run -t uploadfs
"""

import json
import sys
from pathlib import Path
from typing import Dict, List, Tuple

try:
    from PIL import Image, ImageDraw, ImageFont
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
ROOT          = Path(__file__).parent.parent
INPUT_PATH    = ROOT / "data" / "dua-dhikr.json"      # source of truth
OUTPUT_DIR    = ROOT / "esp32-firmware" / "data"
FONTS_DIR     = Path(__file__).parent / "fonts"
ARABIC_FONT   = FONTS_DIR / "NotoNaskhArabic-Bold.ttf"
LATIN_FONT    = FONTS_DIR / "NotoSans-Regular.ttf"

# ── Display geometry ─────────────────────────────────────────────────────────
DISPLAY_W = 800
DISPLAY_H = 480
MARGIN_X  = 40
MARGIN_Y  = 24
USABLE_W  = DISPLAY_W - 2 * MARGIN_X      # 720
USABLE_H  = DISPLAY_H - 2 * MARGIN_Y      # 432
SECTION_GAP = 28                          # space between Arabic block and translation

# ── Arabic sizing ──────────────────────────────────────────────────────────────
ARABIC_SIZE_MAX = 104
ARABIC_SIZE_MIN = 22
ARABIC_STEP     = 4
ARABIC_LINE_GAP = 16                      # extra px between Arabic lines (tashkeel room)
ARABIC_BUDGET   = int(USABLE_H * 0.60)    # Arabic may use up to 60% of usable height

# ── Translation sizing ──────────────────────────────────────────────────────────
TRANS_SIZE_MAX = 30
TRANS_SIZE_MIN = 12
TRANS_LINE_GAP = 6

# Load the Arabic font once for HarfBuzz — reading the file is the slow part.
_hb_blob = hb.Blob.from_file_path(str(ARABIC_FONT))
_hb_face = hb.Face(_hb_blob)

# Caches keyed by pixel size.
_latin_cache: Dict[int, ImageFont.FreeTypeFont] = {}


# ── Arabic font helpers ──────────────────────────────────────────────────────────

def _hb_font(size: int) -> hb.Font:
    """HarfBuzz font scaled so positions are in 26.6 FP (>>6 gives pixels)."""
    f = hb.Font(_hb_face)
    f.scale = (size * 64, size * 64)
    hb.ot_font_set_funcs(f)
    return f


def _ft_face(size: int) -> freetype.Face:
    """FreeType face at size pt, 72 DPI (1 pt = 1 px)."""
    f = freetype.Face(str(ARABIC_FONT))
    f.set_char_size(size * 64)
    return f


def _latin_font(size: int) -> ImageFont.FreeTypeFont:
    if size not in _latin_cache:
        _latin_cache[size] = ImageFont.truetype(str(LATIN_FONT), size)
    return _latin_cache[size]


# ── Arabic shaping ────────────────────────────────────────────────────────────────

def _shape(text: str, font: hb.Font):
    """
    Shape Arabic text with HarfBuzz.

    Returns glyphs in VISUAL LEFT-TO-RIGHT order. Base glyphs have positive
    x_advance; marks (kasra, fatha …) have x_advance=0 and x_offset/y_offset set
    by GPOS so they don't collide with letter dots.
    """
    buf = hb.Buffer()
    buf.add_str(text)
    buf.guess_segment_properties()
    hb.shape(font, buf)
    return buf.glyph_infos, buf.glyph_positions


def _arabic_width_px(text: str, font: hb.Font) -> int:
    _, positions = _shape(text, font)
    return sum(p.x_advance for p in positions) >> 6


def _wrap_arabic(text: str, font: hb.Font) -> List[str]:
    """Wrap Arabic into lines fitting USABLE_W (logical order; HB reorders RTL)."""
    words = text.split()
    lines: List[str] = []
    current = ""
    for word in words:
        candidate = (current + " " + word).strip() if current else word
        if _arabic_width_px(candidate, font) <= USABLE_W:
            current = candidate
        else:
            if current:
                lines.append(current)
            current = word
    if current:
        lines.append(current)
    return lines


def _arabic_line_height(size: int) -> int:
    ft = _ft_face(size)
    return ((ft.size.ascender - ft.size.descender) >> 6) + ARABIC_LINE_GAP


# ── Latin wrapping ─────────────────────────────────────────────────────────────

def _wrap_latin(text: str, font: ImageFont.FreeTypeFont) -> List[str]:
    """Greedy word wrap of the translation to fit USABLE_W."""
    words = text.split()
    lines: List[str] = []
    current = ""
    for word in words:
        candidate = (current + " " + word).strip() if current else word
        if font.getbbox(candidate)[2] <= USABLE_W:
            current = candidate
        else:
            if current:
                lines.append(current)
            current = word
    if current:
        lines.append(current)
    return lines


def _latin_line_height(font: ImageFont.FreeTypeFont) -> int:
    ascent, descent = font.getmetrics()
    return ascent + descent + TRANS_LINE_GAP


# ── Joint layout fit ───────────────────────────────────────────────────────────

class Layout:
    def __init__(self, a_lines, a_size, a_lh, a_h,
                 t_lines, t_size, t_lh, t_h):
        self.a_lines, self.a_size, self.a_lh, self.a_h = a_lines, a_size, a_lh, a_h
        self.t_lines, self.t_size, self.t_lh, self.t_h = t_lines, t_size, t_lh, t_h


def find_layout(arabic: str, translation: str) -> Layout:
    """
    Pick the largest Arabic size whose block fits ARABIC_BUDGET, then the largest
    translation size whose block fits the remaining height. Shrinking the Arabic
    frees room for the translation, so we step Arabic down until both fit.
    """
    best_no_trans = None  # fallback if translation never fits

    for a_size in range(ARABIC_SIZE_MAX, ARABIC_SIZE_MIN - 1, -ARABIC_STEP):
        a_font  = _hb_font(a_size)
        a_lines = _wrap_arabic(arabic, a_font)
        a_lh    = _arabic_line_height(a_size)
        a_h     = len(a_lines) * a_lh - ARABIC_LINE_GAP
        if a_h > ARABIC_BUDGET:
            continue

        if best_no_trans is None:
            best_no_trans = (a_lines, a_size, a_lh, a_h)

        avail = USABLE_H - a_h - SECTION_GAP
        if not translation:
            return Layout(a_lines, a_size, a_lh, a_h, [], 0, 0, 0)

        for t_size in range(TRANS_SIZE_MAX, TRANS_SIZE_MIN - 1, -1):
            t_font  = _latin_font(t_size)
            t_lines = _wrap_latin(translation, t_font)
            t_lh    = _latin_line_height(t_font)
            t_h     = len(t_lines) * t_lh - TRANS_LINE_GAP
            if t_h <= avail:
                return Layout(a_lines, a_size, a_lh, a_h,
                              t_lines, t_size, t_lh, t_h)

    # Nothing fit cleanly — fall back to smallest Arabic + smallest translation.
    a_font  = _hb_font(ARABIC_SIZE_MIN)
    a_lines = _wrap_arabic(arabic, a_font)
    a_lh    = _arabic_line_height(ARABIC_SIZE_MIN)
    a_h     = len(a_lines) * a_lh - ARABIC_LINE_GAP
    if best_no_trans is not None:
        a_lines, a_size, a_lh, a_h = best_no_trans
    else:
        a_size = ARABIC_SIZE_MIN
    t_font  = _latin_font(TRANS_SIZE_MIN)
    t_lines = _wrap_latin(translation, t_font) if translation else []
    t_lh    = _latin_line_height(t_font)
    t_h     = (len(t_lines) * t_lh - TRANS_LINE_GAP) if t_lines else 0
    return Layout(a_lines, a_size, a_lh, a_h,
                  t_lines, TRANS_SIZE_MIN if t_lines else 0, t_lh, t_h)


# ── Rendering ──────────────────────────────────────────────────────────────────

def _render_arabic_line(img: Image.Image, text: str,
                        font: hb.Font, ft: freetype.Face,
                        cx: int, baseline_y: int) -> None:
    """Draw one shaped Arabic line centred at cx, using HarfBuzz GPOS positions."""
    infos, positions = _shape(text, font)
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
            ox = pos.x_offset >> 6
            oy = pos.y_offset >> 6   # negative = below baseline (kasra …)
            gx = cursor_x + ox + ft.glyph.bitmap_left
            gy = baseline_y - oy - ft.glyph.bitmap_top
            for row in range(bm.rows):
                for col in range(bm.width):
                    if bm.buffer[row * bm.pitch + col] > 127:
                        px, py = gx + col, gy + row
                        if 0 <= px < img.width and 0 <= py < img.height:
                            pixels[px, py] = 0   # black on white

        cursor_x += pos.x_advance >> 6


def render_dua(arabic: str, translation: str, output_path: Path) -> Tuple[int, int]:
    """Render Arabic + translation onto an 800×480 1-bit BMP."""
    lay = find_layout(arabic, translation)

    a_font   = _hb_font(lay.a_size)
    ft       = _ft_face(lay.a_size)
    ascender = ft.size.ascender >> 6

    total_h = lay.a_h
    if lay.t_lines:
        total_h += SECTION_GAP + lay.t_h
    start_y = MARGIN_Y + (USABLE_H - total_h) // 2
    if start_y < MARGIN_Y:
        start_y = MARGIN_Y

    img  = Image.new("1", (DISPLAY_W, DISPLAY_H), 1)   # white background

    # Arabic block
    for i, line in enumerate(lay.a_lines):
        baseline_y = start_y + i * lay.a_lh + ascender
        _render_arabic_line(img, line, a_font, ft, DISPLAY_W // 2, baseline_y)

    draw = ImageDraw.Draw(img)
    arabic_bottom = start_y + lay.a_h

    # Divider + translation block
    if lay.t_lines:
        div_y = arabic_bottom + SECTION_GAP // 2
        draw.line([(DISPLAY_W // 2 - 110, div_y),
                   (DISPLAY_W // 2 + 110, div_y)], fill=0, width=2)

        t_font  = _latin_font(lay.t_size)
        ascent, _ = t_font.getmetrics()
        t_start = arabic_bottom + SECTION_GAP
        for i, line in enumerate(lay.t_lines):
            w = draw.textlength(line, font=t_font)
            x = (DISPLAY_W - w) / 2
            y = t_start + i * lay.t_lh
            draw.text((x, y), line, font=t_font, fill=0)

    img.save(str(output_path))
    return lay.a_size, lay.t_size


# ── Entry point ────────────────────────────────────────────────────────────────

def main() -> None:
    print(f"Reading: {INPUT_PATH}")
    with open(INPUT_PATH, encoding="utf-8") as f:
        entries = json.load(f)

    # Wipe stale BMPs so a shorter list never leaves orphans behind.
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    for old in OUTPUT_DIR.glob("dua_*.bmp"):
        old.unlink()

    print(f"Rendering {len(entries)} dua images → {OUTPUT_DIR}")
    print()

    for idx, entry in enumerate(entries):
        arabic = entry.get("arabic", "").strip()
        if not arabic:
            print(f"  [{idx:03d}] SKIP — no arabic text")
            continue
        translation = (entry.get("translation") or "").strip()

        output_path = OUTPUT_DIR / f"dua_{idx:03d}.bmp"
        a_size, t_size = render_dua(arabic, translation, output_path)
        kb = output_path.stat().st_size / 1024
        print(f"  [{idx:03d}] ar {a_size:3d}pt / en {t_size:2d}pt  {kb:.1f} KB"
              f"  — {entry.get('title', '')[:42]}")

    bmp_files = list(OUTPUT_DIR.glob("dua_*.bmp"))
    total_kb  = sum(f.stat().st_size for f in bmp_files) / 1024
    print()
    print(f"Done. {len(bmp_files)} images, {total_kb:.0f} KB ({total_kb/1024:.1f} MB)")
    print()
    print("Next step: flash the filesystem to the ESP32:")
    print("  cd ../esp32-firmware && pio run -t uploadfs")


if __name__ == "__main__":
    main()
