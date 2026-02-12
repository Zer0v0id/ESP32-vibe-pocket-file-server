# Vibe Pocket File Server

**Vibe Pocket File Server** runs on an ESP32 (or ESP32-S3) so you can **browse**, **download**, **upload**, and **delete** files on a microSD card from a phone or laptop. The device runs as a **Wi‑Fi Access Point (softAP)**—no router or internet required.

## Features

- **SD / microSD storage** – Files live on a card connected via SPI.
- **Web interface** – Open a browser, go to the device’s IP, and use the file list and actions.
- **Browse** – Navigate folders and open files (directories show as links).
- **Download** – Click a file to download it.
- **Upload** – Choose a file and upload; it’s saved under the **`files`** folder on the SD (or a path you choose in the URL).
- **Delete** – Delete files from the list (with confirmation).
- **New folder** – Create a directory in the current path from the web UI (POST `/mkdir`).
- **Path breadcrumbs** – The current path is shown as clickable links so you can jump back to any parent directory.
- **Captive portal** – When a client joins the AP, DNS (port 53) resolves all hostnames to the device, and common captive-detection URLs (e.g. `/generate_204`, `/hotspot-detect.html`) redirect to the file server so phones/tablets may open the file browser automatically.
- **No SD card** – If the SD card is not mounted, the server still starts and shows a built-in “No SD card mounted” page so you can connect and see the message instead of a connection error.
- **Standalone AP** – The board creates its own Wi‑Fi network; connect your phone or laptop to it and open the server URL.

## Hardware

- **ESP32 or ESP32-S3** (any module with WiFi). Default wiring below is for **ESP32-S3**.
- **microSD card** (formatted FAT32) and a **microSD breakout** (SPI).

### Wiring (SPI) — ESP32-S3

| SD breakout | ESP32-S3 |
|------------|----------|
| GND        | GND      |
| VCC / 3.3V | 3.3V     |
| MISO       | GPIO 12  |
| MOSI       | GPIO 11  |
| SCK / CLK  | GPIO 13  |
| CS         | GPIO 5   |

Pins are set in `main/main.c`; change `PIN_NUM_*` if your board uses different pins.

## Build and run

1. Install [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/) (version 5.x recommended).
2. **Activate the ESP-IDF environment** so `idf.py` is available:
   - **Windows:** Open the “ESP-IDF x.x PowerShell” or “ESP-IDF x.x CMD” shortcut from the Start Menu (created by the installer), or from a shell run the `export.ps1` / `export.bat` script inside your ESP-IDF installation directory.
   - **Linux / macOS:** Run `source $IDF_PATH/export.sh` (or add it to your shell profile).
   You should see the environment activate (e.g. “Activating ESP-IDF …”, `IDF_PATH` set).
3. In the project directory (from that same shell):

   ```bash
   idf.py set-target esp32s3
   idf.py build
   idf.py -p <PORT> flash monitor
   ```
   Replace `<PORT>` with your serial port (e.g. `COM3` on Windows, `/dev/ttyUSB0` on Linux). Use `esp32` instead of `esp32s3` for classic ESP32 boards.

4. Connect your phone/laptop to Wi‑Fi **SSID:** `Vibe Pocket`, **Password:** `esp32files`, then open **http://192.168.4.1** in a browser.

**Optional:** The repo includes **`build-and-flash.ps1`** (PowerShell). Run it from any PowerShell; it will prompt for the project directory and (if needed) your ESP-IDF path, then remove the `build` folder, run `idf.py fullclean`, set target to `esp32s3`, and build. When the build succeeds, the script tells you to run the flash command yourself (e.g. `idf.py flash` or `idf.py -p COMx flash`).

## Configuration

Edit `main/main.c`:

- **Wi‑Fi AP:** `WIFI_AP_SSID`, `WIFI_AP_PASSWORD`
- **SD pins:** `PIN_NUM_MISO`, `PIN_NUM_MOSI`, `PIN_NUM_CLK`, `PIN_NUM_CS`
- **Web root folder:** `WEB_ROOT_DIR` (default **`files`**; the site opens in `/sdcard/files`)
- **Upload limit:** `MAX_FILE_SIZE` (default 4 MB)

## Editing the web UI on the SD card

The file-browser page can be served from the SD card so you can change the layout and styles without reflashing.

1. On the SD card (at the root) create a folder: **`www`**
2. Copy **`web_template/www/index.html`** from this project into that folder (so you have **`/sdcard/www/index.html`**). The template is at the SD root; the file browser itself works under **`/sdcard/files/`**.
3. Edit `index.html` on the SD card (on a PC or via the upload feature). You can change HTML, CSS, and the upload script.
4. The server injects the file list where it finds **`{{FILE_LIST}}`**, the current path at **`{{CURRENT_PATH}}`**, and the raw path value for the “New folder” form at **`{{CURRENT_PATH_VALUE}}`**. Keep those placeholders in the template if you use them.
5. If the file is missing or invalid, the server falls back to the built-in page.

**Speed/safety:** Serving the page from SD is slightly slower than the built-in version (one small file read per request). For a typical browser session it’s negligible. The firmware always keeps an embedded fallback, so the device still works if the SD file is removed or corrupted.

## Notes

- The site opens in **`/sdcard/files`** by default (the `files` directory is created automatically if missing). Browsing and uploads stay under this folder so you’re not dropped at the SD root.
- First run: ensure the SD card is inserted and formatted FAT32 before powering the board for full file-server use.
- **If the SD card fails to mount** (wrong wiring, bad card, or no card), the HTTP server still starts. Opening http://192.168.4.1 shows a built-in **“No SD card mounted”** page; upload and delete return an error. Insert a card and reset to use the file server.
- The server uses chunked transfer for large files to limit RAM use. Filenames with spaces or special characters are supported (URL-encoded by the browser and sanitized for FAT).

## Troubleshooting

### Build fails at `sections.ld` / ldgen (e.g. errorcode -1073741819 on Windows)

This usually means the linker script generator (`ldgen.py`) crashed (access violation) or hit a Python/pyparsing error. Try:

1. **Use the same environment as your IDE**  
   Always build from the **ESP-IDF 5.5 PowerShell** (or the shell where you ran `Initialize-Idf.ps1`). Don’t mix a normal PowerShell with an ESP-IDF one.

2. **Full clean and rebuild**  
   Delete the **`build`** folder, run **`idf.py fullclean`**, then **`idf.py set-target esp32s3`** and **`idf.py build`** again.

3. **Shorter project path**  
   On Windows, the ldgen command line can be very long. If your path is long (e.g. `D:\DEV\ESP_File_Server`), try building from a shorter path (e.g. **`D:\ESP\FS`**) by copying or cloning the project there and building.

4. **Reinstall or repair the ESP-IDF Python env**  
   The crash can be due to an incompatible **pyparsing** (or Python) version in the IDF virtualenv. Re-run the ESP-IDF installer and choose “Repair” or “Reconfigure Python environment”, or recreate the IDF 5.5 env with the Python version recommended for your IDF version.

5. **See the real error**  
   From the project directory, run the same command that failed (from **`build/CMakeFiles/sections.ld-*.bat`**), or run **`idf.py build`** and check **`build/log/idf_py_stderr_output_*.log`** for a Python traceback. That message (e.g. pyparsing or `TypeError`) will point to the exact fix (often a dependency or Python version).

## License

Unlicense / CC0-1.0.
