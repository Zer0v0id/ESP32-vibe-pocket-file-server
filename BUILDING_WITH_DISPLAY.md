# Quick Start: Building with SSD1306 Display

## What Was Added

Your Vibe Pocket File Server now supports an **SSD1306 128x64 OLED display** via I2C! The display shows:

- WiFi SSID (network name)
- IP address (192.168.4.1)
- SD card status
- Server URL

## Wiring Your Display

Connect your SSD1306 OLED to the ESP32-S3:

| Display Pin | ESP32-S3 Pin |
|-------------|--------------|
| GND         | GND          |
| VCC         | 3.3V         |
| SCL (SCK)   | GPIO 9       |
| SDA         | GPIO 8       |

## Building and Flashing

### Prerequisites
Make sure you have ESP-IDF 5.x installed. If not, follow: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/

### Build Steps

1. **Activate ESP-IDF environment:**
   
   **Windows:**
   ```powershell
   # Open "ESP-IDF 5.x PowerShell" from Start Menu
   ```
   
   **Linux/macOS:**
   ```bash
   source $IDF_PATH/export.sh
   ```

2. **Navigate to project directory:**
   ```bash
   cd /path/to/ESP32-vibe-pocket-file-server
   ```

3. **Checkout the display branch:**
   ```bash
   git fetch origin
   git checkout cursor/add-ssd1306-oled-display-3dc1
   ```

4. **Set target and build:**
   ```bash
   idf.py set-target esp32s3
   idf.py build
   ```

5. **Flash to your ESP32:**
   ```bash
   idf.py -p COM3 flash monitor
   # Replace COM3 with your port (e.g., /dev/ttyUSB0 on Linux)
   ```

6. **Watch it boot!**
   - The display should initialize and show status information
   - Connect to WiFi "Vibe Pocket" (password: esp32files)
   - Open http://192.168.4.1 in your browser

## Troubleshooting

### Display stays blank?

1. **Check wiring** - Especially VCC (3.3V) and GND
2. **Check I2C address** - Some displays use 0x3D instead of 0x3C
   - Edit `main/display.c`, line 24:
     ```c
     #define SSD1306_I2C_ADDR    0x3D  // Change from 0x3C
     ```
3. **Look at serial monitor** - You should see "SSD1306 OLED display initialized"

### Need different GPIO pins?

Edit `main/display.c`, lines 15-16:
```c
#define I2C_MASTER_SCL_IO    9   // Your SCL pin
#define I2C_MASTER_SDA_IO    8   // Your SDA pin
```

Then rebuild with `idf.py build`.

## Release Build (Optimized)

For production/faster performance:

```bash
idf.py -DCMAKE_BUILD_TYPE=Release build
```

Or use the Windows batch file:
```cmd
build-release.bat
```

## Create Single Flash File

After building, create a single binary:

```bash
python merge_flash_bin.py
```

Then flash with:
```bash
esptool.py -p COM3 write_flash 0x0 build/vibe_pocket_file_server_flash.bin
```

## Testing Checklist

- [ ] Display shows "Vibe Pocket" title
- [ ] SSID shows correctly
- [ ] IP shows as "192.168.4.1"
- [ ] SD status shows "SD: OK" (with card) or "SD: NO CARD" (without)
- [ ] URL displays at bottom
- [ ] Web interface still works at http://192.168.4.1

## No Display? No Problem!

The display is **completely optional**. If you don't connect it:
- The server will attempt to initialize it (you'll see an error in logs)
- Everything else continues to work normally
- The web interface is fully functional

## Need Help?

See the full documentation:
- `DISPLAY_WIRING.md` - Complete wiring guide
- `README.md` - Project overview
- `CONTRIBUTING.md` - Development info

## Files Changed

- `main/display.c` - Full SSD1306 driver implementation (333 lines)
- `main/display.h` - Updated header with I2C details
- `DISPLAY_WIRING.md` - New comprehensive wiring guide
- `README.md` - Added display mention

Enjoy your new OLED display! 🎉
