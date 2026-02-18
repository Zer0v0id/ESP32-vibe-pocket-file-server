/*
 * Status display: stub (no display on ESP32-S3 build).
 */

#include "display.h"
#include "esp_err.h"

int display_status_init(void)
{
    return ESP_OK;
}

void display_status_update(const char *ssid, const char *ip_str, bool sd_ok, const char *url)
{
    (void)ssid;
    (void)ip_str;
    (void)sd_ok;
    (void)url;
}
