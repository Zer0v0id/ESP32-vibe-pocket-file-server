#pragma once

#include "display.h"
#include "esp_err.h"
#include "status_led.h"

#define THEME_DARK  0
#define THEME_LIGHT 1

typedef struct {
    char wifi_ssid[33];
    char wifi_pass[65];
    uint8_t wifi_channel;
    uint8_t wifi_max_conn;
    char web_root_dir[32];
    uint32_t max_file_size;
    char sta_ssid[33];
    char sta_pass[65];
    uint8_t theme;
    uint8_t mobile_default;
    uint8_t led_mode;
    uint8_t disp_on;
    uint8_t disp_contrast;
    uint8_t disp_invert;
    uint8_t disp_rotate;
    uint8_t enc_rev;         /* 1 = reverse encoder scroll */
    uint8_t disp_line[DISPLAY_LINE_MAX];
} app_config_t;

void app_display_refresh(void);
app_config_t *app_config_get(void);
esp_err_t app_settings_save(void);
void app_settings_apply_live(void);
const char *app_led_mode_label(uint8_t mode);
bool app_sd_mounted(void);
bool app_sd_needs_format(void);
/** FAT32 format the SD card and create the web-root folders. Erases all files. */
esp_err_t app_sd_format_init(void);
