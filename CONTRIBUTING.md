# Contributing to Vibe Pocket File Server

Thank you for your interest in contributing to the Vibe Pocket File Server project!

## Getting Started

1. **Fork the repository** on GitHub
2. **Clone your fork** locally
3. **Create a branch** for your feature or bug fix
4. **Make your changes** with clear commit messages
5. **Test your changes** on actual ESP32-S3 hardware if possible
6. **Submit a pull request** with a clear description

## Development Setup

### Prerequisites

- ESP-IDF version 5.x (recommended)
- ESP32-S3 development board
- microSD card (FAT32 formatted)
- microSD SPI breakout module

### Building

```bash
# Activate ESP-IDF environment
source $IDF_PATH/export.sh  # Linux/macOS
# or use ESP-IDF PowerShell on Windows

# Set target
idf.py set-target esp32s3

# Build
idf.py build

# Flash
idf.py -p <PORT> flash monitor
```

## Code Style Guidelines

- **Indentation**: 4 spaces (no tabs)
- **Line length**: Aim for 120 characters max
- **Comments**: Use clear, concise comments for non-obvious code
- **Naming**:
  - Functions: `snake_case`
  - Constants: `UPPER_SNAKE_CASE`
  - Variables: `snake_case`
  - Structs: `snake_case_t` suffix

## Testing

- Test on real hardware when possible
- Verify SD card operations (read, write, delete, rename)
- Test WiFi connectivity and web interface
- Check memory usage and potential leaks
- Test with various file sizes and types

## What to Contribute

### Bug Fixes
- Memory leaks
- Buffer overflows
- Incorrect error handling
- UI issues

### Features
- File operations (copy, move, zip)
- Advanced filtering/search
- Authentication/authorization
- Additional file format support
- Performance optimizations

### Documentation
- Code comments
- README improvements
- Usage examples
- Troubleshooting guides

## Submitting Changes

1. **Commit messages** should be descriptive:
   ```
   Add file sorting to web interface
   
   - Sort files alphabetically by name
   - Directories appear before files
   - Added sort parameter to directory listing
   ```

2. **Pull request description** should include:
   - What changes were made
   - Why the changes were necessary
   - How to test the changes
   - Screenshots (for UI changes)

3. **Code review**: Be responsive to feedback and willing to make adjustments

## Questions?

- Open an issue for questions or discussions
- Check existing issues before creating new ones

## License

By contributing, you agree that your contributions will be licensed under the same license as the project (Unlicense / CC0-1.0).
