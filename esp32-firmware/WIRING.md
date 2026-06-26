# Wiring Guide: ESP32-S3 + Waveshare 7.3" (F) 7-Color Display

## Display Connector Pins

The Waveshare 7.3" HAT has an 8-pin connector:

```
Display Pin | Function | ESP32-S3 Pin | GPIO
------------|----------|--------------|------
VCC         | Power 3.3V| 3V3         | -
GND         | Ground   | GND         | -
DIN         | MOSI     | Pin 11      | GPIO 11
CLK         | SCK      | Pin 12      | GPIO 12
CS          | Chip Select | Pin 10   | GPIO 10
DC          | Data/Command | Pin 8   | GPIO 8
RST         | Reset    | Pin 9       | GPIO 9
BUSY        | Busy Signal | Pin 7    | GPIO 7
```

## Visual Wiring Diagram

```
ESP32-S3 DevKit                  Waveshare 7.3" Display
┌──────────────────┐            ┌─────────────────────┐
│                  │            │                     │
│ 3V3 (Power) ─────┼────────────┼─→ VCC              │
│ GND (Ground)─────┼────────────┼─→ GND              │
│                  │            │                     │
│ GPIO 11 (MOSI)───┼────────────┼─→ DIN              │
│ GPIO 12 (SCK)────┼────────────┼─→ CLK              │
│ GPIO 10 (CS)─────┼────────────┼─→ CS               │
│ GPIO 8  (DC)─────┼────────────┼─→ DC               │
│ GPIO 9  (RST)────┼────────────┼─→ RST              │
│ GPIO 7  (BUSY)───┼────────────┼─→ BUSY             │
│                  │            │                     │
│ GPIO 13 (MISO)   │            │ (not connected)     │
└──────────────────┘            └─────────────────────┘
```

## Push Button (GPIO 2)

One momentary push button wakes the device from deep sleep to toggle the page.
(Data refresh is fully automatic — the device triggers the update itself on its
daily wake — so no refresh button is needed.)

| Button | GPIO | Function | Wake source |
|--------|------|----------|-------------|
| Page button | GPIO 2 | Toggle prayer ⇄ dua page (no WiFi) | EXT1, active-LOW |

The button is **active-LOW**: the pin idles **HIGH** (held up by a pull-up
resistor) and is driven **LOW** when the button is pressed. The firmware wakes on
that LOW level (`ESP_EXT1_WAKEUP_ANY_LOW`).

### Schematic (with pull-up resistor)

```text
   3V3
    │
  [ 10 kΩ ]      ← pull-up resistor (pin idles HIGH)
    │
    ├─────────●  GPIO 2   → to ESP32
    │
  (button)       ← momentary push button
    │
   GND           ← pressing connects the pin to GND (reads LOW)
```

### Wiring steps

1. **3V3** → one leg of a **10 kΩ** resistor.
2. Other leg of the resistor → **GPIO 2**. This junction is the signal.
3. **GPIO 2** → one terminal of the **push button**.
4. Other terminal of the push button → **GND**.

### Notes

- **The internal pull-up is also enabled in firmware** (`rtc_gpio_pullup_en`), so the
  button will function even without the external resistor. The external **10 kΩ**
  pull-up is **recommended** for a cleaner, more reliable signal — especially for
  dependable wake-from-deep-sleep and noise immunity on longer wires.
- **Optional debounce:** a **100 nF** capacitor from GPIO 2 to GND suppresses
  contact bounce (reduces accidental double-triggers). Not required for wake.
- Use any standard **normally-open** tactile push button.
- ⚠️ Use **GPIO 2** — it is RTC-capable (required for EXT1 deep-sleep wake) and
  avoids the display/strapping pins.

### Parts

- 1× momentary push button (normally open)
- 1× 10 kΩ resistor (pull-up)
- Optional: 1× 100 nF capacitor (debounce)
- Jumper wires

## Connection Steps

### 1. Prepare Materials
- 8x Female-to-Female jumper wires (or use the cable that came with the HAT)
- ESP32-S3 board
- Waveshare 7.3" display
- USB-C cable (for ESP32-S3)

### 2. Important: Power Off First!
**Disconnect USB before wiring!**

### 3. Connect Wires One by One

**Power First:**
1. ESP32 `3V3` → Display `VCC` (Red wire recommended)
2. ESP32 `GND` → Display `GND` (Black wire recommended)

**Data Lines:**
3. ESP32 `GPIO 11` → Display `DIN`
4. ESP32 `GPIO 12` → Display `CLK`
5. ESP32 `GPIO 10` → Display `CS`
6. ESP32 `GPIO 8` → Display `DC`
7. ESP32 `GPIO 9` → Display `RST`
8. ESP32 `GPIO 7` → Display `BUSY`

### 4. Double-Check Connections
- ✓ VCC and GND correct? (Wrong polarity can damage components!)
- ✓ All 8 wires connected firmly?
- ✓ No loose connections?

### 5. Power On
- Connect USB-C cable to ESP32-S3
- Upload test code via PlatformIO

## Color Coding (Recommended)

Use colored wires for easier debugging:
- 🔴 Red: VCC (Power 3.3V)
- ⚫ Black: GND (Ground)
- 🟡 Yellow: CLK (Clock)
- 🟢 Green: MOSI/DIN (Data)
- 🔵 Blue: CS (Chip Select)
- 🟣 Purple: DC (Data/Command)
- 🟠 Orange: RST (Reset)
- ⚪ White: BUSY (Status)

## Troubleshooting

### Display Not Responding
- Check power: Is VCC connected to 3.3V (NOT 5V)?
- Check ground: Is GND connected?
- Check BUSY pin: Display won't work without it

### Garbled Display
- Check CLK and DIN pins
- Verify CS and DC pins
- Try different SPI speed (in code)

### Display Shows Nothing
- Check RST pin (display needs reset on startup)
- Verify all wires are secure
- Check serial monitor for error messages

## Safety Notes

⚠️ **Important:**
- Waveshare e-ink displays use **3.3V logic** - Do NOT connect to 5V!
- ESP32-S3 GPIO pins are also 3.3V - Perfect match
- Maximum current: ~40mA during refresh
- Total power: ~130mW (safe for USB power)

## Next Steps

After wiring:
1. Open project in PlatformIO
2. Build and upload test code
3. Open Serial Monitor (115200 baud)
4. Watch display refresh with color bars (~30 seconds)
5. If successful, move to data display implementation

## Questions?

If display doesn't work:
1. Check serial monitor output
2. Verify wiring with multimeter (check continuity)
3. Try a simpler test (just initialize, no graphics)
