/*
 * Status display (stub for ESP32-S3 build; no built-in display).
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize the display (backlight, LCD). Call once after WiFi/SD init.
 * Returns ESP_OK on success.
 */
int display_status_init(void);

/**
 * Update status text on the display.
 * Pass NULL for any string to leave that line unchanged (or clear if first call).
 */
void display_status_update(const char *ssid, const char *ip_str, bool sd_ok, const char *url);

#ifdef __cplusplus
}
#endif
