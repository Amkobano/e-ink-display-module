# ESP32 Firmware for E-Ink Display

## Hardware
- **MCU:** ESP32-S3 DevKit
- **Display:** Waveshare 7.3" e-Paper HAT
- **Power:** Lithium battery (with deep sleep)

## Development

### Prerequisites
- PlatformIO IDE (VS Code extension)
- USB-C cable
- ESP32-S3 drivers (usually automatic on macOS)

### Setup
1. Open this folder in VS Code with PlatformIO
2. Configure WiFi credentials in `src/main.cpp`
3. Update GitHub URL with your repository
4. Build and upload to ESP32-S3

### PlatformIO Commands
```bash
# Build
pio run

# Upload to ESP32
pio run --target upload

# Monitor serial output
pio device monitor

# Upload and monitor
pio run --target upload && pio device monitor
```

### Pin Connections
(Will be added once display model is confirmed)


## Power Consumption
- Active (WiFi + Display update): ~200mA for 30-60 seconds
- Deep sleep: ~10-20μA
- Expected battery life: 1-3 months on 3000mAh battery (2 updates/day)
