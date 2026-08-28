# SSD1306 OLED Display - Wiring Diagram

```
┌─────────────────────────────────────────────────────────────────┐
│                                                                 │
│                    ESP32-S3 Development Board                   │
│                                                                 │
│   ┌──────────┐                                                  │
│   │   3.3V   ├──────────────────────┐                          │
│   └──────────┘                      │                          │
│                                     │                          │
│   ┌──────────┐                      │                          │
│   │   GND    ├────────────┐         │                          │
│   └──────────┘            │         │                          │
│                           │         │                          │
│   ┌──────────┐            │         │                          │
│   │ GPIO 8   ├────┐       │         │                          │
│   │  (SDA)   │    │       │         │                          │
│   └──────────┘    │       │         │                          │
│                   │       │         │                          │
│   ┌──────────┐    │       │         │                          │
│   │ GPIO 9   ├──┐ │       │         │                          │
│   │  (SCL)   │  │ │       │         │                          │
│   └──────────┘  │ │       │         │                          │
│                 │ │       │         │                          │
└─────────────────┼─┼───────┼─────────┼──────────────────────────┘
                  │ │       │         │
                  │ │       │         │
                  │ │       │         │
            ┌─────┼─┼───────┼─────────┼──────────┐
            │     │ │       │         │          │
            │   ┌─┴─┴─┐   ┌─┴─┐     ┌─┴──┐       │
            │   │ SCL │   │SDA│     │VCC │       │
            │   │(SCK)│   │   │     │    │       │
            │   └─────┘   └───┘     └────┘       │
            │   ┌─────┐                           │
            │   │ GND │                           │
            │   └──┬──┘                           │
            │      │                              │
            │  ┌───┴───────────────────────────┐  │
            │  │                               │  │
            │  │    SSD1306 OLED Display       │  │
            │  │         128 x 64              │  │
            │  │                               │  │
            │  │  ┌─────────────────────────┐  │  │
            │  │  │                         │  │  │
            │  │  │                         │  │  │
            │  │  │      [OLED SCREEN]      │  │  │
            │  │  │                         │  │  │
            │  │  │                         │  │  │
            │  │  └─────────────────────────┘  │  │
            │  │                               │  │
            │  └───────────────────────────────┘  │
            │                                     │
            └─────────────────────────────────────┘

```

## Connection Summary

| Connection | Color (typical) | Description                    |
|------------|-----------------|--------------------------------|
| VCC → 3.3V | Red             | Power supply (3.3V)           |
| GND → GND  | Black           | Ground                         |
| SCL → IO9  | Yellow/Green    | I2C Clock signal              |
| SDA → IO8  | Blue/White      | I2C Data signal               |

## Pin Functions

**ESP32-S3 Side:**
- **GPIO 8 (SDA)**: I2C Data line - carries bidirectional data
- **GPIO 9 (SCL)**: I2C Clock line - provides timing/synchronization
- **3.3V**: Power output for the display
- **GND**: Common ground reference

**SSD1306 Display Side:**
- **VCC**: Power input (3.3V or 5V depending on module)
- **GND**: Ground
- **SDA**: I2C data line (connects to GPIO 8)
- **SCL/SCK**: I2C clock line (connects to GPIO 9)

## Notes

1. **Pull-up Resistors**: Most SSD1306 modules include built-in pull-up resistors (typically 4.7kΩ or 10kΩ) on SDA and SCL. The ESP32 also enables internal pull-ups by default.

2. **Voltage**: Most SSD1306 modules work with 3.3V, but some can handle 5V. Check your module specifications. The ESP32-S3 outputs 3.3V which is safe for all modules.

3. **I2C Address**: This display uses address **0x3C** (default). Some modules use 0x3D - check if display doesn't initialize.

4. **Cable Length**: Keep I2C wires short (< 20cm recommended) for reliable communication at 400kHz. Longer cables may require lower speeds.

5. **Multiple I2C Devices**: If you want to add more I2C devices (sensors, etc.), they can share the same SDA/SCL bus if they have different addresses.

## Breadboard Layout Example

```
     ESP32-S3              Breadboard              SSD1306
                             Rails
    
    3.3V ───────────────── + Rail ──────────────── VCC
    GND  ───────────────── - Rail ──────────────── GND
    GPIO8 ───────────────────────────────────────── SDA
    GPIO9 ───────────────────────────────────────── SCL
```

## Testing

After wiring, when you power on the ESP32-S3:
1. The display should light up (backlight on)
2. After ~1 second, you should see text appear
3. Display shows: WiFi name, IP address, SD status

If nothing appears, check:
- [ ] VCC connected to 3.3V (not 5V, not GND)
- [ ] GND connected properly
- [ ] SDA on GPIO 8 (not swapped with SCL)
- [ ] SCL on GPIO 9 (not swapped with SDA)
- [ ] Display I2C address (try 0x3D if 0x3C fails)
