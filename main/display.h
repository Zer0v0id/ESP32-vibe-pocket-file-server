/*
 * Status display.
 *
 * Default firmware (CONFIG_DISPLAY_SSD1306 unset): no-op stub.
 * OLED firmware: SSD1306 over I2C (SDA/SCL from Kconfig).
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#define DISPLAY_CONTRAST_LOW     0
#define DISPLAY_CONTRAST_MED     1
#define DISPLAY_CONTRAST_HIGH    2

/* Max rows we store in NVS. 128x32 uses 4; 128x64 uses 8. */
#define DISPLAY_LINE_MAX         8

enum {
    DISP_FIELD_BLANK = 0,
    DISP_FIELD_TITLE,
    DISP_FIELD_SSID,
    DISP_FIELD_IP,
    DISP_FIELD_URL,
    DISP_FIELD_SD,
    DISP_FIELD_SD_FREE,
    DISP_FIELD_SD_TOTAL,
    DISP_FIELD_FILES,
    DISP_FIELD_CLIENTS,
    DISP_FIELD_CLIENTS_MAX,
    DISP_FIELD_UPTIME,
    DISP_FIELD_HEAP,
    DISP_FIELD_PSRAM,
    DISP_FIELD_VERSION,
    DISP_FIELD_CHANNEL,
    DISP_FIELD_STA_IP,
    DISP_FIELD_MAC,
    DISP_FIELD_CHIP,
    DISP_FIELD_WIFI_MODE,
    DISP_FIELD_WEB_ROOT,
    DISP_FIELD_TEMP,
    DISP_FIELD_MAX_UPLOAD,
    DISP_FIELD_RESET,
    DISP_FIELD_SD_OK,
    DISP_FIELD_AP_CH,
    DISP_FIELD_COUNT
};

#ifdef __cplusplus
extern "C" {
#endif

/** Apply Settings values before display_status_init(). No-op without OLED firmware. */
void display_configure(uint8_t enabled, uint8_t contrast, uint8_t invert, uint8_t rotate180);

/** Apply current display_configure values without rebooting. */
void display_apply(void);

/**
 * Initialize the status display.
 * With OLED firmware: sets up I2C and the SSD1306.
 * Without: returns ESP_OK immediately.
 */
int display_status_init(void);

/**
 * Draw one preformatted string per OLED row (21 chars typical).
 * Extra rows are ignored on 128x32.
 */
void display_status_update(const char *const *lines, unsigned count);

#ifdef __cplusplus
}
#endif
