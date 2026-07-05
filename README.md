# Vibe Pocket File Server

Vibe Pocket File Server runs on an ESP32-S3 so you can browse, download, upload, and delete files on a microSD card from a phone or laptop. The device runs as a Wi‑Fi Access Point (softAP)—no router or internet required.

Features
SD / microSD storage – Files live on a card connected via SPI.
​

Web interface – Open a browser, go to the device IP, and use the file list and actions.
​

Browse – Navigate folders and open files; directories show as links.
​

Download – Click a file in the list to download it.
​

Upload – Choose a file and upload it; it is saved under the files folder on the SD card (or a path you choose in the URL).
​

Delete – Delete files from the list (with confirmation).
​

New folder – Create a directory in the current path from the web UI (POST /mkdir).
​

Path breadcrumbs – The current path is shown as clickable links so you can jump back to any parent directory.
​

Captive portal – When a client joins the AP, DNS (port 53) resolves all hostnames to the device, and common captive-detection URLs (for example /generate_204, /hotspot-detect.html) redirect to the file server so phones and tablets may open the file browser automatically.
​

No SD card – If the SD card is not mounted, the server still starts and shows a built-in “No SD card mounted” page so you see a clear message instead of a connection error.
​

Standalone AP – The board creates its own Wi‑Fi network; connect your phone or laptop and open the server URL.
​

Settings page – Open http://192.168.4.1/settings to change Wi‑Fi SSID, password, AP channel, max connections, optional join network (STA mode: device also connects to your router for LAN access), theme (Dark / Light), default view (Desktop / Mobile), web root folder name, and max upload size. Settings are stored in NVS and persist across reboots; saving triggers a reboot so new Wi‑Fi settings take effect. A Reboot now action restarts the device without changing settings.
​

Status page – Open http://192.168.4.1/status to see uptime, free heap, SD card free/total space, connected AP clients, STA IP (when joined to a network), and firmware version.
​

Theme – Choose Dark or Light in Settings; the theme applies to the file list, Settings, and Status pages.
​

Mobile view – Set Default view to Mobile for larger touch targets and a mobile-friendly layout, or use the Mobile view / Desktop view link on any page to switch for the current session.
​

Rename – Rename files and folders from the file list by entering a new name and clicking Rename in that row.
​

SD space in UI – The file browser shows SD: X MB free / Y MB (or “not mounted”) above the file table.
​

Hardware
ESP32-S3 (any module with Wi‑Fi support).
​

microSD card (formatted FAT32) and a microSD SPI breakout.
​

Wiring (SPI) — ESP32-S3
SD breakout	ESP32-S3
GND	GND
VCC / 3.3V	3.3V
MISO	GPIO 12
MOSI	GPIO 11
SCK / CLK	GPIO 13
CS	GPIO 5
Pins are set in main/main.c; change PIN_NUM_* if your board uses different pins.
​

Build and run
Install ESP-IDF (version 5.x recommended).
​

Activate the ESP-IDF environment so idf.py is available:
​

Windows: Open the “ESP-IDF x.x PowerShell” or “ESP-IDF x.x CMD” shortcut from the Start Menu (created by the installer), or run the export.ps1 / export.bat script inside your ESP-IDF installation directory.
​

Linux / macOS: Run source $IDF_PATH/export.sh (or add it to your shell profile).
​

In the project directory (from that same shell), run:

```bash
idf.py set-target esp32s3
idf.py build
idf.py -p <PORT> flash monitor
```

Replace `<PORT>` with your serial port (for example COM3 on Windows or /dev/ttyUSB0 on Linux).

**Release build (optimized for ESP32-S3):** From the same ESP-IDF shell, run:

```bash
idf.py -DCMAKE_BUILD_TYPE=Release build
```

Or use **build-release.bat** (Windows): it activates IDF with the project’s Python env, builds release, then merges bootloader + partition table + app into a single image.

**Single flash image:** After any build, run `python merge_flash_bin.py` to create `build/vibe_pocket_file_server_flash.bin`. Flash that one file with:

```bash
esptool.py -p <PORT> write_flash 0x0 build/vibe_pocket_file_server_flash.bin
```

Otherwise use `idf.py -p <PORT> flash` to flash the separate binaries.
​

Connect your phone or laptop to the device Wi‑Fi (default SSID Vibe Pocket, password esp32files; you can change this at http://192.168.4.1/settings), then open http://192.168.4.1 in a browser.
​

Optional: The repo includes build-and-flash.ps1 (PowerShell). Run it from any PowerShell; it prompts for the project directory and (if needed) your ESP-IDF path, removes the build folder, runs idf.py fullclean, sets target to esp32s3, and builds. When the build succeeds, the script tells you which flash command to run (for example idf.py flash or idf.py -p COMx flash).
​

Configuration
From the web: Open http://192.168.4.1/settings to change Wi‑Fi SSID, password, AP channel (1–13), max connections (1–10), optional join network (STA SSID and password so the device also connects to your router), theme (Dark / Light), default view (Desktop / Mobile), web root folder name, and max upload size (1–32 MB). Changes are saved to flash and the device reboots after saving so the new Wi‑Fi settings apply.
​

In code (for SD pins only; the rest is set in Settings): edit main/main.c and set PIN_NUM_MISO, PIN_NUM_MOSI, PIN_NUM_CLK, and PIN_NUM_CS if your wiring differs.
​

Editing the web UI on the SD card
The file-browser page can be served from the SD card so you can change the layout and styles without reflashing.
​

On the SD card root, create a folder: www.
​

Copy web_template/www/index.html from this project into that folder so you have /sdcard/www/index.html. The template lives at the SD root; the file browser itself serves content under /sdcard/files/.
​

Edit index.html on the SD card (on a PC or via the upload feature). You can change the HTML, CSS, and upload script.
​

The server injects the file list at {{FILE_LIST}}, the current path at {{CURRENT_PATH}}, and the raw path value for the “New folder” form at {{CURRENT_PATH_VALUE}}. Keep those placeholders if you want those dynamic values.
​

If the file is missing or invalid, the server falls back to the built-in page.
​

Speed and safety: Serving the page from SD is slightly slower than the built-in version (one small file read per request), but for typical use it is negligible. The firmware always keeps an embedded fallback, so the device still works if the SD file is removed or corrupted.
​

Notes
The site opens in /sdcard/files by default; the files directory is created automatically if it is missing.
​

Browsing and uploads stay under this folder so you are not dropped at the SD root.
​

On first run, insert and format the SD card as FAT32 before powering the board for full file-server use.
​

If the SD card fails to mount (wrong wiring, bad card, or no card), the HTTP server still starts. Opening http://192.168.4.1 shows a built-in “No SD card mounted” page; upload and delete operations return an error until you insert a card and reset.
​

The server uses chunked transfer for large files to limit RAM use; filenames with spaces or special characters are supported (URL-encoded by the browser and sanitized for FAT).
​

Files and folders in directory listings are sorted alphabetically with directories appearing first for easier navigation.
​

Security headers (X-Content-Type-Options, X-Frame-Options, Cache-Control) are included in HTTP responses for improved security.
​

Troubleshooting
Build fails at sections.ld / ldgen (for example error code -1073741819 on Windows)
This usually means the linker script generator (ldgen.py) crashed (access violation) or hit a Python/pyparsing error. Try:
​

Use the ESP-IDF environment
Always build from the ESP-IDF 5.x PowerShell (or the shell where you ran the ESP-IDF export script). Do not mix a normal shell with an ESP-IDF one.
​

Full clean and rebuild
Delete the build folder, run idf.py fullclean, then idf.py set-target esp32s3 and idf.py build again.
​

Shorter project path
On Windows, the ldgen command line can be very long. If your project path is long, copy or clone the project to a shorter path (for example from D:\DEV\ESP_File_Server to D:\ESP\FS) and build there.
​

Repair the ESP-IDF Python environment
The crash can be due to an incompatible pyparsing or Python version in the IDF virtualenv. Re-run the ESP-IDF installer and choose “Repair” or “Reconfigure Python environment”, or recreate the IDF 5.x environment with the recommended Python version.
​

Check the real error
From the project directory, run the same command that failed (see build/CMakeFiles/sections.ld-*.bat), or run idf.py build and inspect build/log/idf_py_stderr_output_*.log for a Python traceback. That message (for example a pyparsing TypeError) points to the exact fix.
​
## License

Unlicense / CC0-1.0.
