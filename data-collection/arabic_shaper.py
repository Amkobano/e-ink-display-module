"""
arabic_shaper.py — Pre-shape Arabic text for ESP32 e-ink display rendering.

U8g2 (used on the ESP32) cannot perform Arabic ligature shaping or RTL reordering.
This script runs offline once to pre-process dua-dhikr.json:
  - Reshapes each Arabic string so letters connect correctly (arabic-reshaper)
  - Reverses the string to LTR display order (python-bidi)
  - Adds an `arabic_shaped` field to each entry
  - Writes the result to esp32-firmware/data/dua-dhikr.json (ready for LittleFS upload)

Usage:
    pip install arabic-reshaper python-bidi
    python arabic_shaper.py

Input:  ../data/dua-dhikr.json
Output: ../esp32-firmware/data/dua-dhikr.json
"""

import json
import sys
from pathlib import Path

try:
    import arabic_reshaper
    from bidi.algorithm import get_display
except ImportError:
    print("Missing dependencies. Run:")
    print("  pip install arabic-reshaper python-bidi")
    sys.exit(1)

INPUT_PATH = Path(__file__).parent.parent / "data" / "dua-dhikr.json"
OUTPUT_PATH = Path(__file__).parent.parent / "esp32-firmware" / "data" / "dua-dhikr.json"


def shape_arabic(text: str) -> str:
    """Reshape Arabic text and apply bidi algorithm for LTR rendering."""
    reshaped = arabic_reshaper.reshape(text)
    return get_display(reshaped)


def main():
    print(f"Reading: {INPUT_PATH}")
    with open(INPUT_PATH, encoding="utf-8") as f:
        entries = json.load(f)

    print(f"Processing {len(entries)} entries...")
    for entry in entries:
        arabic = entry.get("arabic", "")
        if arabic:
            entry["arabic_shaped"] = shape_arabic(arabic)
        else:
            entry["arabic_shaped"] = ""

    OUTPUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    with open(OUTPUT_PATH, "w", encoding="utf-8") as f:
        json.dump(entries, f, ensure_ascii=False, indent=2)

    print(f"Written: {OUTPUT_PATH}")
    print()
    print("Sample output (first entry):")
    print(f"  arabic:        {entries[0]['arabic']}")
    print(f"  arabic_shaped: {entries[0]['arabic_shaped']}")
    print()
    print("Next step: flash the filesystem to ESP32:")
    print("  cd esp32-firmware && pio run -t uploadfs")


if __name__ == "__main__":
    main()
