/*
 * Status display.
 *
 * Generic DevKit: SSD1306 over I2C when CONFIG_DISPLAY_SSD1306 is set.
 * LilyGO T-Embed CC1101: ST7789 320x170 when CONFIG_DISPLAY_ST7789 is set.
 * Otherwise a no-op stub.
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

/** Apply Settings values before display_status_init(). */
void display_configure(uint8_t enabled, uint8_t contrast, uint8_t invert, uint8_t rotate180);

/** Apply current display_configure values without rebooting. */
void display_apply(void);

/**
 * Initialize the status display (SSD1306, ST7789, or no-op).
 */
int display_status_init(void);

/**
 * Draw one preformatted string per status row.
 * Extra rows are ignored on 128x32 OLED.
 */
void display_status_update(const char *const *lines, unsigned count);

/** True while the on-device menu owns the panel (skip status refresh). */
bool display_ui_busy(void);
void display_ui_set_busy(bool busy);

uint16_t display_rgb(uint8_t r, uint8_t g, uint8_t b);
void display_clear(uint16_t color);
void display_fill_rect(int x, int y, int w, int h, uint16_t color);
void display_text(int x, int y, const char *s, uint16_t fg, uint16_t bg);
int display_font_w(void);
int display_font_h(void);

#ifdef __cplusplus
}
#endif
