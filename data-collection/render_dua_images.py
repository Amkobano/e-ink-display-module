"""
render_dua_images.py — Pre-render each dua's Arabic text as an 800×480 BMP image.

Replaces the U8g2 runtime text rendering on the ESP32, which is limited to a 16px
Arabic font. This script uses Pillow + NotoNaskhArabic + arabic-reshaper + python-bidi
to render each dua at the largest font size that still fits the display, then saves
the result as a 1-bit monochrome BMP directly into the LittleFS data directory.

Usage:
    pip install Pillow arabic-reshaper python-bidi
    python render_dua_images.py

Input:  ../data/dua-dhikr.json
Output: ../esp32-firmware/data/dua_000.bmp … dua_083.bmp
        (overwrites existing files)

After running, flash the filesystem:
    cd ../esp32-firmware && pio run -t uploadfs
"""

import json
import sys
from pathlib import Path
from typing import List, Tuple

try:
    from PIL import Image, ImageDraw, ImageFont
except ImportError:
    print("Missing dependency: pip install Pillow")
    sys.exit(1)

try:
    import arabic_reshaper
    from bidi.algorithm import get_display
except ImportError:
    print("Missing dependencies: pip install arabic-reshaper python-bidi")
    sys.exit(1)

# ── Paths ──────────────────────────────────────────────────────────────────────
ROOT       = Path(__file__).parent.parent
INPUT_PATH = ROOT / "data" / "dua-dhikr.json"
OUTPUT_DIR = ROOT / "esp32-firmware" / "data"
FONT_PATH  = Path(__file__).parent / "fonts" / "NotoNaskhArabic-Bold.ttf"

# ── Display constants ──────────────────────────────────────────────────────────
DISPLAY_W  = 800
DISPLAY_H  = 480
MARGIN_X   = 30   # horizontal margin on each side
MARGIN_Y   = 20   # vertical margin top and bottom
USABLE_W   = DISPLAY_W - 2 * MARGIN_X
USABLE_H   = DISPLAY_H - 2 * MARGIN_Y
LINE_GAP   = 8    # extra px between lines (on top of font ascent+descent)

# Font size search: try from largest down, step of 4
FONT_SIZE_MAX = 120
FONT_SIZE_MIN = 14
FONT_SIZE_STEP = 4


def prepare_arabic(text: str) -> str:
    """Reshape Arabic text and apply bidi for correct visual rendering."""
    reshaped = arabic_reshaper.reshape(text)
    return get_display(reshaped)


def wrap_text(text: str, font: ImageFont.FreeTypeFont, max_width: int) -> List[str]:
    """
    Wrap text into lines that fit within max_width pixels using the given font.
    Splits on spaces; single words wider than max_width go on their own line.
    """
    words = text.split()
    lines: List[str] = []
    current = ""

    for word in words:
        candidate = f"{current} {word}".strip() if current else word
        bbox = font.getbbox(candidate)
        w = bbox[2] - bbox[0]
        if w <= max_width:
            current = candidate
        else:
            if current:
                lines.append(current)
            current = word
    if current:
        lines.append(current)
    return lines


def measure_block(
    lines: List[str], font: ImageFont.FreeTypeFont
) -> Tuple[int, int]:
    """Return (max_line_width, total_height) for a wrapped block."""
    ascent, descent = font.getmetrics()
    line_h = ascent + descent + LINE_GAP
    total_h = len(lines) * line_h - LINE_GAP  # no extra gap after last line
    max_w = max((font.getbbox(l)[2] - font.getbbox(l)[0]) for l in lines)
    return max_w, total_h


def find_best_fit(arabic_text: str) -> Tuple[List[str], ImageFont.FreeTypeFont, int]:
    """
    Find the largest font size where all wrapped lines fit within USABLE_W × USABLE_H.
    Returns (lines, font, font_size).
    """
    for size in range(FONT_SIZE_MAX, FONT_SIZE_MIN - 1, -FONT_SIZE_STEP):
        font = ImageFont.truetype(str(FONT_PATH), size)
        lines = wrap_text(arabic_text, font, USABLE_W)
        _, total_h = measure_block(lines, font)
        if total_h <= USABLE_H:
            return lines, font, size

    # Fallback: minimum size (should always fit)
    font = ImageFont.truetype(str(FONT_PATH), FONT_SIZE_MIN)
    lines = wrap_text(arabic_text, font, USABLE_W)
    return lines, font, FONT_SIZE_MIN


def render_dua(arabic_text: str, output_path: Path) -> int:
    """
    Render arabic_text onto an 800×480 1-bit BMP, vertically and horizontally centered.
    Returns the font size used.
    """
    lines, font, font_size = find_best_fit(arabic_text)
    ascent, descent = font.getmetrics()
    line_h = ascent + descent + LINE_GAP
    _, total_h = measure_block(lines, font)

    # 1-bit image: 1 = white (background), 0 = black (text)
    img = Image.new("1", (DISPLAY_W, DISPLAY_H), 1)
    draw = ImageDraw.Draw(img)

    # Vertically center the block
    start_y = MARGIN_Y + (USABLE_H - total_h) // 2

    for i, line in enumerate(lines):
        line_y = start_y + i * line_h
        bbox = font.getbbox(line)
        line_w = bbox[2] - bbox[0]
        # Horizontally center each line
        line_x = (DISPLAY_W - line_w) // 2
        draw.text((line_x, line_y), line, font=font, fill=0)

    img.save(str(output_path))
    return font_size


def main() -> None:
    print(f"Reading: {INPUT_PATH}")
    with open(INPUT_PATH, encoding="utf-8") as f:
        entries = json.load(f)

    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)

    total = len(entries)
    print(f"Rendering {total} dua images to {OUTPUT_DIR} ...")
    print()

    for idx, entry in enumerate(entries):
        arabic_raw = entry.get("arabic", "")
        if not arabic_raw:
            print(f"  [{idx:03d}] SKIP — no arabic text")
            continue

        arabic_shaped = prepare_arabic(arabic_raw)
        output_path = OUTPUT_DIR / f"dua_{idx:03d}.bmp"

        font_size = render_dua(arabic_shaped, output_path)
        file_kb = output_path.stat().st_size / 1024
        print(f"  [{idx:03d}] font={font_size:3d}pt  {file_kb:.1f} KB  — {entry.get('title', '')[:50]}")

    # Summary
    bmp_files = list(OUTPUT_DIR.glob("dua_*.bmp"))
    total_kb = sum(f.stat().st_size for f in bmp_files) / 1024
    print()
    print(f"Done. {len(bmp_files)} images, {total_kb:.0f} KB total ({total_kb/1024:.1f} MB)")
    print()
    print("Next step: flash the filesystem to ESP32:")
    print("  cd ../esp32-firmware && pio run -t uploadfs")


if __name__ == "__main__":
    main()
