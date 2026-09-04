# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- LilyGO T-Embed CC1101 board profile: ST7789 320×170, onboard TF on shared SPI, 8× WS2812, 16MB flash / OPI PSRAM (`idf.py -DBOARD=t-embed-cc1101`)
- T-Embed on-device settings menu (encoder) and a larger 12×20 status font
- SSD1306 OLED status display (I2C) in the default firmware; turn the screen off in Settings
- Configurable OLED lines, contrast, invert, rotate 180°, and quick layouts
- Onboard WS2812 RGB LED: solid colors, breathe, rainbow, heartbeat, blink, sparkle, alternate, and Status
- Settings apply display/LED/theme without reboot; Wi‑Fi changes still reboot
- `DISPLAY_WIRING.md` for OLED pinout and troubleshooting
- File sorting in directory listings (directories first, then alphabetically)
- Security headers (X-Content-Type-Options, X-Frame-Options, Cache-Control)
- `.gitattributes` for proper line ending handling
- `CONTRIBUTING.md` with contribution guidelines
- `CHANGELOG.md` to track project changes

### Changed
- Single firmware image (OLED driver included; no separate with/without-display builds)
- Improved directory listing performance with sorted entries
- Enhanced code organization for better maintainability

### Fixed
- Settings save on phone captive-portal browsers (GET path save, extra HTTP sockets)

## [1.0.0] - Initial Release

### Added
- ESP32-S3 WiFi Access Point file server
- SD card support (FAT32)
- Web interface for file browsing
- File operations: browse, download, upload, delete, rename
- Create new folders from web UI
- Captive portal with DNS server
- Settings page for WiFi configuration
- Theme support (Dark/Light)
- Mobile/Desktop view toggle
- Optional STA mode (join existing network)
- Status page with uptime, heap, SD space, connected clients
- Path breadcrumb navigation
- Upload progress indicator
- NVS storage for persistent settings
- Single binary flash image support
- Build scripts for Windows (PowerShell, Batch)
