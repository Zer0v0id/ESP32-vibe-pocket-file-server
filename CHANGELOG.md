# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- File sorting in directory listings (directories first, then alphabetically)
- Security headers (X-Content-Type-Options, X-Frame-Options, Cache-Control)
- `.gitattributes` for proper line ending handling
- `CONTRIBUTING.md` with contribution guidelines
- `CHANGELOG.md` to track project changes

### Changed
- Improved directory listing performance with sorted entries
- Enhanced code organization for better maintainability

### Fixed
- None

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
