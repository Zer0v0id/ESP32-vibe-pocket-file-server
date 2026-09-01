/*
 * Onboard WS2812 RGB status LED (ESP32-S3 DevKit typically GPIO 48).
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Mode IDs 0–5 are stable (stored in NVS). Newer colors/patterns follow. */
#define STATUS_LED_MODE_OFF            0
#define STATUS_LED_MODE_DIM_GREEN      1
#define STATUS_LED_MODE_DIM_BLUE       2
#define STATUS_LED_MODE_DIM_AMBER      3
#define STATUS_LED_MODE_BREATHE        4  /* green */
#define STATUS_LED_MODE_STATUS         5
#define STATUS_LED_MODE_DIM_RED        6
#define STATUS_LED_MODE_DIM_WHITE      7
#define STATUS_LED_MODE_DIM_PURPLE     8
#define STATUS_LED_MODE_DIM_CYAN       9
#define STATUS_LED_MODE_DIM_PINK       10
#define STATUS_LED_MODE_DIM_YELLOW     11
#define STATUS_LED_MODE_BREATHE_BLUE   12
#define STATUS_LED_MODE_BREATHE_AMBER  13
#define STATUS_LED_MODE_BREATHE_RED    14
#define STATUS_LED_MODE_RAINBOW        15
#define STATUS_LED_MODE_HEARTBEAT      16
#define STATUS_LED_MODE_BLINK_SLOW     17
#define STATUS_LED_MODE_BLINK_FAST     18
#define STATUS_LED_MODE_ALTERNATE      19
#define STATUS_LED_MODE_SPARKLE        20
#define STATUS_LED_MODE_MAX            20

void status_led_init(uint8_t mode);
void status_led_set_mode(uint8_t mode);
void status_led_set_sd_ok(bool ok);

#ifdef __cplusplus
}
#endif
