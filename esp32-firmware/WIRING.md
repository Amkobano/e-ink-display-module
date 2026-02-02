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
