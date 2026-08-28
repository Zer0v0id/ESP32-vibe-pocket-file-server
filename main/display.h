/*
 * Status display: SSD1306 OLED 128x64 I2C driver
 * 
 * Hardware connections (ESP32-S3):
 *   - SDA: GPIO 8
 *   - SCL: GPIO 9
 *   - VCC: 3.3V
 *   - GND: GND
 * 
 * I2C address: 0x3C (default for most SSD1306 modules)
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize the SSD1306 OLED display via I2C.
 * Call once after WiFi/SD init.
 * Returns ESP_OK on success, error code otherwise.
 */
int display_status_init(void);

/**
 * Update status information on the OLED display.
 * Displays: SSID, IP address, SD card status, and URL.
 * Pass NULL for any string to skip that field.
 * 
 * @param ssid WiFi SSID string
 * @param ip_str IP address string (e.g., "192.168.4.1")
 * @param sd_ok true if SD card is mounted, false otherwise
 * @param url URL string (e.g., "http://192.168.4.1")
 */
void display_status_update(const char *ssid, const char *ip_str, bool sd_ok, const char *url);

#ifdef __cplusplus
}
#endif
