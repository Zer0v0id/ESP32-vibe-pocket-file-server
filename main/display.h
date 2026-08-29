/*
 * Status display.
 *
 * Default firmware (CONFIG_DISPLAY_SSD1306 unset): no-op stub.
 * OLED firmware: SSD1306 128x64 over I2C (SDA/SCL from Kconfig).
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize the status display.
 * With OLED firmware: sets up I2C and the SSD1306.
 * Without: returns ESP_OK immediately.
 * Returns ESP_OK on success, or an error if the OLED is enabled but missing.
 */
int display_status_init(void);

/**
 * Update status text on the display.
 * With OLED firmware: SSID, IP, SD card status, and URL.
 * Without: no-op.
 */
void display_status_update(const char *ssid, const char *ip_str, bool sd_ok, const char *url);

#ifdef __cplusplus
}
#endif
