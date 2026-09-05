# LilyGO T-Embed CC1101

Board firmware for the [T-Embed CC1101](https://lilygo.cc/en-us/products/t-embed-cc1101) (ESP32-S3-WROOM-1, 16MB flash, 8MB OPI PSRAM). Same file server as the generic DevKit: Wi‑Fi AP, captive portal, Settings, Status, SD files.

This is **not** the original T-Embed (non-CC1101). LCD and SD GPIOs differ.

The CC1101 radio, PN532 NFC, encoder, IR, and speaker are unused. Radio CS (GPIO 12) is held high so it stays off the shared SPI bus.

## Build and flash

ESP-IDF 5.3.x. Use a **separate** build directory so you do not overwrite a generic DevKit `sdkconfig`.

```bash
source $IDF_PATH/export.sh   # or ESP-IDF PowerShell on Windows
idf.py -B build-t-embed -DBOARD=t-embed-cc1101 set-target esp32s3
idf.py -B build-t-embed -DBOARD=t-embed-cc1101 build
idf.py -B build-t-embed -p <PORT> flash monitor
```

Flash size is 16MB, PSRAM is octal, console is USB Serial/JTAG. Plug in the USB-C port and pick the `usbmodem` / `ttyACM` device.

If the board will not enter download mode, hold the encoder button (GPIO 0) while plugging USB. See [LilyGO download mode](https://github.com/Xinyuan-LilyGO/T-Embed-CC1101/blob/master/docs/download_mode.md).

## Hardware used by this firmware

| Function | GPIO | Notes |
|----------|------|--------|
| Power enable | 15 | Set high at boot (required) |
| SPI SCK | 11 | Shared: LCD + TF |
| SPI MOSI | 9 | Shared |
| SPI MISO | 10 | Shared |
| SD CS | 13 | Onboard TF slot |
| LCD CS | 41 | ST7789 170×320 |
| LCD DC | 16 | |
| LCD BL | 21 | Backlight |
| LCD RST | — | Not wired (software reset) |
| WS2812 | 14 | 8 LEDs |
| CC1101 CS | 12 | Held high; radio unused |

Landscape 320×170 (LilyGO `display.begin(3)`). Status header plus six lines, same Settings fields as the web UI. **Click the encoder** for Settings. Turn to move, click to open or adjust (turn then click to keep), **Home** / **Back** or hold to go up a level. **Display → Scroll** reverses the knob. **Power** can reboot or shut down (PWR_EN off, then deep sleep; plug USB or use the power button to wake). **Storage → Format SD** (click twice) FAT32-formats the card and creates `files/` and `www/`, then reboots. Screen Off in Settings turns the backlight off.

Do **not** drive GPIO 38 or 48 as a DevKit LED. Those pins are CC1101 band select (`SW0`/`IO2`).

## SD card

This TF slot is **not** the DevKit wiring (MISO 12 / MOSI 11 / SCK 13 / CS 5). LCD, TF, CC1101, and the expansion nRF24 header share SPI2:

| Function | GPIO |
|----------|------|
| SCK / MOSI / MISO | 11 / 9 / 10 |
| SD CS | 13 |
| LCD CS | 41 |
| CC1101 CS | 12 (held high) |
| nRF24 CS / CE | 44 / 43 (idle; header only) |

PWR_EN (GPIO 15) must be high before the slot is powered. Firmware waits, idles every CS, skips the ESP-IDF “wait for MISO high” check (that hangs on this shared bus), and clocks the card at 10 MHz then 4 MHz.

If Storage shows **No FAT**, the slot answered but the card has no FAT32 volume (blank, Mac/Windows exFAT, or a leftover partition). **Storage → Format SD** (click twice) writes FAT32 and creates `files/` and `www/`, then reboots. That erases the card.

LilyGO also reports better results with **SanDisk cards 32GB or smaller**. If the slot is empty, the web UI still starts with the “No SD card” page.

## What it does not do

No Sub-GHz TX/RX, NFC, IR, or audio. If you want those, use [LilyGO’s examples](https://github.com/Xinyuan-LilyGO/T-Embed-CC1101) or a project such as Bruce.
