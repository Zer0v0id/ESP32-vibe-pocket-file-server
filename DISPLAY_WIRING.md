# SSD1306 OLED Display Wiring

The Vibe Pocket File Server supports an optional **SSD1306 128x64 OLED display** (I2C) to show status information.

## Hardware Requirements

- SSD1306 OLED display module (128x64 pixels, I2C interface)
- 4 wires for I2C connection

## Wiring Diagram

Connect the SSD1306 OLED display to your ESP32-S3 as follows:

| SSD1306 Pin | ESP32-S3 Pin | Description       |
|-------------|--------------|-------------------|
| GND         | GND          | Ground            |
| VCC         | 3.3V         | Power (3.3V)      |
| SCL (SCK)   | GPIO 9       | I2C Clock         |
| SDA         | GPIO 8       | I2C Data          |

## Pin Configuration

The I2C pins are configured in `main/display.c`:

```c
#define I2C_MASTER_SCL_IO    9    /* GPIO for I2C SCL */
#define I2C_MASTER_SDA_IO    8    /* GPIO for I2C SDA */
```

If you need to use different GPIO pins, modify these definitions in `display.c` before building.

## I2C Address

The default I2C address is **0x3C** (most common for SSD1306 modules).

If your display uses a different address (e.g., 0x3D), change this line in `display.c`:

```c
#define SSD1306_I2C_ADDR     0x3C
```

## Display Information

The OLED display shows:

1. **Title**: "Vibe Pocket"
2. **SSID**: WiFi network name (AP)
3. **IP Address**: Device IP (default: 192.168.4.1)
4. **SD Card Status**: "SD: OK" or "SD: NO CARD"
5. **URL**: Server URL for easy reference

## Build and Flash

After wiring the display, build and flash as usual:

```bash
idf.py build
idf.py -p <PORT> flash monitor
```

The display will initialize automatically on boot and show the current status.

## Troubleshooting

### Display shows nothing / stays blank

1. **Check wiring**: Verify VCC, GND, SDA, and SCL connections
2. **Check I2C address**: Some modules use 0x3D instead of 0x3C
3. **Check power**: Ensure the display gets 3.3V (not 5V on most modules)
4. **Check monitor logs**: Look for "SSD1306 OLED display initialized" message

### Display shows garbled text

1. **I2C speed**: The driver uses 400kHz. If issues persist, lower to 100kHz:
   ```c
   #define I2C_MASTER_FREQ_HZ    100000
   ```

### Pull-up resistors

Most SSD1306 modules include built-in pull-up resistors on SDA and SCL. The driver enables internal pull-ups as well for reliability. If you experience issues, you can:

- Disable internal pull-ups in `display.c`:
  ```c
  .sda_pullup_en = GPIO_PULLUP_DISABLE,
  .scl_pullup_en = GPIO_PULLUP_DISABLE,
  ```

- Add external 4.7kΩ pull-up resistors from SDA/SCL to 3.3V

## No Display Required

The display is **optional**. If no display is connected:

- The device will attempt to initialize it and log a failure
- The file server will continue to work normally
- All functionality remains available via the web interface
