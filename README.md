# Vibe Pocket File Server

Vibe Pocket File Server runs on an ESP32-S3 so you can browse, download, upload, and delete files on a microSD card from a phone or laptop. The device is a Wi‑Fi Access Point (softAP)—no router or internet required.

There is **one firmware**. An SSD1306 OLED is supported out of the box. If you have no screen, leave it unwired. If you have one and want it dark, turn it off in Settings.

## Features

- **SD / microSD storage** — Files live on a FAT32 card connected via SPI.
- **Web interface** — Open a browser, go to the device IP, and use the file list and actions.
- **Browse / download / upload / delete / rename** — Folders are links; uploads go under the `files` folder on the SD card (or a path you choose).
- **New folder** — Create a directory in the current path from the web UI (`POST /mkdir`).
- **Path breadcrumbs** — Jump back to any parent directory.
- **Captive portal** — DNS (port 53) resolves all hostnames to the device. Common captive-detection URLs (`/generate_204`, `/hotspot-detect.html`) redirect to the file server so phones may open it automatically.
- **No SD card** — The server still starts and shows a built-in “No SD card mounted” page.
- **Standalone AP** — Connect to the board’s Wi‑Fi and open the server URL.
- **Settings** — [http://192.168.4.1/settings](http://192.168.4.1/settings): Wi‑Fi SSID, password, AP channel, max connections, optional join-network (STA), theme, default view, RGB LED, OLED (on/off, contrast, invert, rotate, line fields), web root, max upload size. Stored in NVS. Display, LED, and theme apply immediately. Changing Wi‑Fi name, password, channel, or join-network settings reboots. **Reboot device** restarts without saving the form.
- **Status** — [http://192.168.4.1/status](http://192.168.4.1/status): uptime, heap, SD space, AP clients, STA IP, firmware version.
- **Theme** — Dark or Light, applied to the file list, Settings, and Status.
- **RGB LED** — Onboard WS2812 (GPIO 48 on most ESP32-S3 DevKits; some use GPIO 38). Default is dim green. Settings: off, solid colors (green/blue/amber/red/white/purple/cyan/pink/yellow), breathe, rainbow, heartbeat, blink, alternate, sparkle, or Status (red = no SD, green = idle, cyan = a client is connected). Brightness stays low.
- **OLED** — Optional SSD1306 (I2C). Four configurable status lines on 128×32 (eight on 128×64). Turn the screen **Off** in Settings if you do not want it. See [DISPLAY_WIRING.md](DISPLAY_WIRING.md).
- **Mobile view** — Larger touch targets, or switch with the Mobile view / Desktop view link.
- **SD space in UI** — `SD: X MB free / Y MB` above the file table.

## Hardware

- ESP32-S3 (any module with Wi‑Fi). 8MB PSRAM boards work well.
- microSD card (FAT32) and a microSD SPI breakout.
- Optional SSD1306 OLED (I2C), typically 128×32 (0.91") or 128×64 (0.96").

### SD card (SPI)

| SD breakout | ESP32-S3 |
|-------------|----------|
| GND | GND |
| VCC / 3.3V | 3.3V |
| MISO | GPIO 12 |
| MOSI | GPIO 11 |
| SCK / CLK | GPIO 13 |
| CS | GPIO 5 |

Pins are set in `main/main.c` (`PIN_NUM_*`).

### OLED (I2C), optional

| SSD1306 | ESP32-S3 |
|---------|----------|
| GND | GND |
| VCC | 3.3V |
| SDA | GPIO 18 |
| SCL / SCK | GPIO 17 |

Change pins and 128×32 vs 128×64 in `idf.py menuconfig` → *Board configuration*. Do not use GPIO 46 (input-only), GPIO 38/48 (RGB LED), or the SD SPI pins.

## Build and flash

Install [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/) 5.x.

Activate the environment so `idf.py` is on your PATH:

- **Windows:** Open “ESP-IDF x.x PowerShell” / CMD, or run `export.ps1` / `export.bat` in the IDF install.
- **Linux / macOS:** `source $IDF_PATH/export.sh`

From the project directory:

```bash
idf.py set-target esp32s3
idf.py build
idf.py -p <PORT> flash monitor
```

Replace `<PORT>` with your serial port (for example `COM3` on Windows, `/dev/ttyUSB0` on Linux, `/dev/cu.usbmodem*` on macOS).

**Release build:**

```bash
idf.py -DCMAKE_BUILD_TYPE=Release build
```

On Windows, `build-release.bat` activates IDF, builds release, and merges a single flash image.

**Single flash image:** after a build, `python merge_flash_bin.py` writes `build/vibe_pocket_file_server_flash.bin`. Flash it with:

```bash
esptool.py -p <PORT> write_flash 0x0 build/vibe_pocket_file_server_flash.bin
```

Or use `idf.py -p <PORT> flash`.

`build-and-flash.ps1` (PowerShell) prompts for the project directory and ESP-IDF path, cleans, sets target, and builds. It prints the flash command when the build succeeds.

Connect to Wi‑Fi **Vibe Pocket** (password `esp32files`; change this in Settings), then open [http://192.168.4.1](http://192.168.4.1).

If a phone says “No Internet”, that is normal. Type `192.168.4.1` in the browser. Prefer Safari or Chrome over the Wi‑Fi login popup if Settings will not save.

## Configuration

**From the web:** [http://192.168.4.1/settings](http://192.168.4.1/settings)

- Wi‑Fi SSID, password, AP channel (1–13), max connections (1–10)
- Optional join network (STA SSID and password)
- Theme, default view, RGB LED pattern
- OLED: on/off, contrast, invert, rotate 180°, per-line fields or a quick layout
- Web root folder name, max upload size (1–32 MB)

Display, LED, and theme apply as soon as you save. Wi‑Fi / join-network changes reboot.

**In code:** SD pins in `main/main.c`. OLED SDA/SCL, I2C address, and panel size in `idf.py menuconfig`.

## Editing the web UI on the SD card

The file-browser page can be served from the SD card so you can change layout and styles without reflashing.

1. On the SD card root, create `www`.
2. Copy `web_template/www/index.html` from this project to `/sdcard/www/index.html`. The file browser itself serves content under `/sdcard/files/`.
3. Edit that file (HTML, CSS, upload script). Keep `{{FILE_LIST}}`, `{{CURRENT_PATH}}`, and `{{CURRENT_PATH_VALUE}}`.

If the file is missing or invalid, the firmware falls back to the built-in page.

## Notes

- The site opens in `/sdcard/files` by default; that directory is created if missing.
- Browse and upload stay under this folder so you are not dropped at the SD root.
- Format the SD card as FAT32 before first use.
- If the card fails to mount, HTTP still starts; upload and delete error until you insert a card and reset.
- Large files use chunked transfer. Names with spaces or special characters are URL-encoded and sanitized for FAT.
- Directory listings are sorted alphabetically, directories first.
- Responses include `X-Content-Type-Options`, `X-Frame-Options`, and `Cache-Control`.

## Troubleshooting

**Build fails at `sections.ld` / ldgen** (for example error `-1073741819` on Windows): the linker script generator crashed. Build from the ESP-IDF 5.x shell only. Delete `build`, run `idf.py fullclean`, then `idf.py set-target esp32s3` and `idf.py build`. On Windows, a shorter project path can help. Repair the IDF Python environment if `pyparsing` errors appear.

**OLED is blank:** check 3.3V (not 5V), SDA=18 / SCL=17, address 0x3C vs 0x3D, and 128×32 vs 128×64 in menuconfig. Settings → Screen must be On. Serial log reports if the panel was not found.

**Save on Settings does nothing:** open Safari or Chrome to `http://192.168.4.1/settings` instead of the phone’s Wi‑Fi popup.

## License

Unlicense / CC0-1.0.
