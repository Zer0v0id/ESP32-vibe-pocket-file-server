# SSD1306 OLED wiring (optional)

The firmware includes the OLED driver. If no screen is connected, the file server still runs. Turn the panel off anytime in **Settings → Screen → Off**.

## Pins (ESP32-S3)

| OLED pin | ESP32-S3 | Notes |
|----------|----------|--------|
| GND | GND | Ground |
| VCC | 3.3V | 3.3V, not 5V |
| SDA | GPIO 18 | Configurable in `idf.py menuconfig` |
| SCL (SCK) | GPIO 17 | Configurable in `idf.py menuconfig` |

Do not use **GPIO 46**. On ESP32-S3 it is input-only and cannot drive I2C. That produces a black or garbled screen.

Default firmware is **128×32** (0.91 inch). For a 0.96 inch 128×64 panel, set *OLED resolution* in `idf.py menuconfig`.

Do not reuse the SD SPI pins: GPIO 5 (CS), 11 (MOSI), 12 (MISO), 13 (SCK).

Do not use **GPIO 38** or **GPIO 48** for I2C. Those pins drive the onboard RGB LED on typical ESP32-S3 DevKits; using them as SCL/SDA can blast the LED at full white.

Default I2C address is **0x3C**. If the screen stays blank, try **0x3D** under *Board configuration* in `idf.py menuconfig`.

## What it shows

Each of the four lines (eight on 128×64) is chosen in Settings: SSID, IP, URL, SD, clients, uptime, heap, and more. Quick layouts (status, compact, network, system, storage) fill the lines for you.

## Build

```bash
idf.py set-target esp32s3
idf.py build
idf.py -p <PORT> flash monitor
```

If no OLED is connected, the serial log reports that the display was not found and the web UI still works.
