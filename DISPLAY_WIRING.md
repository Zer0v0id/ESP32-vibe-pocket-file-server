# SSD1306 OLED wiring (optional)

Use this only for the **with-display** firmware. The default build has no OLED driver.

## Pins (ESP32-S3)

| OLED pin | ESP32-S3 | Notes |
|----------|----------|--------|
| GND | GND | Ground |
| VCC | 3.3V | 3.3V, not 5V |
| SDA | GPIO 8 | Configurable in `idf.py menuconfig` |
| SCL (SCK) | GPIO 9 | Configurable in `idf.py menuconfig` |

Do not reuse the SD SPI pins: GPIO 5 (CS), 11 (MOSI), 12 (MISO), 13 (SCK).

Default I2C address is **0x3C**. If the screen stays blank, try **0x3D** under *Board configuration* in `idf.py menuconfig`.

## What it shows

- Title: Vibe Pocket
- Wi‑Fi SSID
- IP address
- SD card status
- Server URL (host part)

## Build

```bash
idf.py -B build-display -DWITH_DISPLAY=ON set-target esp32s3
idf.py -B build-display -DWITH_DISPLAY=ON build
idf.py -B build-display -p <PORT> flash monitor
```

If no OLED is connected, this firmware still runs the file server. The serial log reports that the display was not found.
