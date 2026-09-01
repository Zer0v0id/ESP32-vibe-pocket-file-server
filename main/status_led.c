/*
 * WS2812 RGB status LED via led_strip (RMT).
 */

#include "sdkconfig.h"
#include "status_led.h"
#include "esp_log.h"

#if CONFIG_STATUS_LED

#include "led_strip.h"
#include "esp_err.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "status_led";

static led_strip_handle_t s_strips[2];
static int s_nstrips;
static volatile uint8_t s_mode = STATUS_LED_MODE_DIM_GREEN;
static volatile bool s_sd_ok;
static bool s_ready;

static uint8_t dim(uint8_t v)
{
    uint32_t b = (uint32_t)CONFIG_STATUS_LED_BRIGHTNESS;
    if (b > 64) {
        b = 64;
    }
    return (uint8_t)((v * b) / 64);
}

static void led_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    uint8_t dr = dim(r), dg = dim(g), db = dim(b);
    for (int i = 0; i < s_nstrips; i++) {
        led_strip_set_pixel(s_strips[i], 0, dr, dg, db);
        led_strip_refresh(s_strips[i]);
    }
}

static void led_off(void)
{
    for (int i = 0; i < s_nstrips; i++) {
        led_strip_clear(s_strips[i]);
        led_strip_refresh(s_strips[i]);
    }
}

static bool led_solid_rgb(uint8_t mode, uint8_t *r, uint8_t *g, uint8_t *b)
{
    switch (mode) {
    case STATUS_LED_MODE_DIM_GREEN:
        *r = 0; *g = 255; *b = 0;
        return true;
    case STATUS_LED_MODE_DIM_BLUE:
        *r = 0; *g = 0; *b = 255;
        return true;
    case STATUS_LED_MODE_DIM_AMBER:
        *r = 255; *g = 80; *b = 0;
        return true;
    case STATUS_LED_MODE_DIM_RED:
        *r = 255; *g = 0; *b = 0;
        return true;
    case STATUS_LED_MODE_DIM_WHITE:
        *r = 255; *g = 255; *b = 255;
        return true;
    case STATUS_LED_MODE_DIM_PURPLE:
        *r = 160; *g = 0; *b = 255;
        return true;
    case STATUS_LED_MODE_DIM_CYAN:
        *r = 0; *g = 220; *b = 255;
        return true;
    case STATUS_LED_MODE_DIM_PINK:
        *r = 255; *g = 40; *b = 140;
        return true;
    case STATUS_LED_MODE_DIM_YELLOW:
        *r = 255; *g = 200; *b = 0;
        return true;
    default:
        return false;
    }
}

static void led_apply_solid(uint8_t mode)
{
    if (s_nstrips == 0) {
        return;
    }
    if (mode == STATUS_LED_MODE_OFF) {
        led_off();
        return;
    }
    uint8_t r, g, b;
    if (led_solid_rgb(mode, &r, &g, &b)) {
        led_rgb(r, g, b);
        return;
    }
    led_rgb(0, 255, 0);
}

static void led_breathe(uint8_t r, uint8_t g, uint8_t b, unsigned tick)
{
    unsigned phase = tick % 40;
    unsigned tri = phase < 20 ? phase : 40 - phase; /* 0..20 */
    uint16_t s = 8 + (uint16_t)tri * 12;            /* 8..248 */
    led_rgb((uint8_t)((r * s) / 255),
            (uint8_t)((g * s) / 255),
            (uint8_t)((b * s) / 255));
}

static void hsv_to_rgb(uint8_t h, uint8_t *r, uint8_t *g, uint8_t *b)
{
    uint8_t region = (uint8_t)(h / 43);
    uint8_t remainder = (uint8_t)((h - (region * 43)) * 6);
    uint8_t q = (uint8_t)((255 * (255 - remainder)) >> 8);
    uint8_t t = (uint8_t)((255 * remainder) >> 8);
    switch (region) {
    case 0:  *r = 255; *g = t;   *b = 0;   break;
    case 1:  *r = q;   *g = 255; *b = 0;   break;
    case 2:  *r = 0;   *g = 255; *b = t;   break;
    case 3:  *r = 0;   *g = q;   *b = 255; break;
    case 4:  *r = t;   *g = 0;   *b = 255; break;
    default: *r = 255; *g = 0;   *b = q;   break;
    }
}

static void led_heartbeat(unsigned tick)
{
    unsigned t = tick % 36; /* ~1.8s */
    unsigned tri = 0;
    if (t < 6) {
        tri = t;
    } else if (t < 12) {
        tri = 12 - t;
    } else if (t >= 16 && t < 22) {
        tri = t - 16;
    } else if (t >= 22 && t < 28) {
        tri = 28 - t;
    }
    uint8_t s = (uint8_t)(tri * 42);
    led_rgb(s, 0, 0);
}

static void led_task(void *arg)
{
    (void)arg;
    unsigned tick = 0;
    uint32_t spark = 0xA5A5u;
    while (1) {
        uint8_t mode = s_mode;
        switch (mode) {
        case STATUS_LED_MODE_BREATHE:
            led_breathe(0, 255, 0, tick);
            break;
        case STATUS_LED_MODE_BREATHE_BLUE:
            led_breathe(0, 0, 255, tick);
            break;
        case STATUS_LED_MODE_BREATHE_AMBER:
            led_breathe(255, 80, 0, tick);
            break;
        case STATUS_LED_MODE_BREATHE_RED:
            led_breathe(255, 0, 0, tick);
            break;
        case STATUS_LED_MODE_RAINBOW: {
            uint8_t r, g, b;
            hsv_to_rgb((uint8_t)(tick * 2), &r, &g, &b);
            led_rgb(r, g, b);
            break;
        }
        case STATUS_LED_MODE_HEARTBEAT:
            led_heartbeat(tick);
            break;
        case STATUS_LED_MODE_BLINK_SLOW:
            if ((tick / 10) % 2) {
                led_rgb(255, 255, 255);
            } else {
                led_off();
            }
            break;
        case STATUS_LED_MODE_BLINK_FAST:
            if ((tick / 3) % 2) {
                led_rgb(255, 255, 255);
            } else {
                led_off();
            }
            break;
        case STATUS_LED_MODE_ALTERNATE:
            if ((tick / 8) % 2) {
                led_rgb(0, 220, 255);
            } else {
                led_rgb(255, 0, 180);
            }
            break;
        case STATUS_LED_MODE_SPARKLE:
            if (tick % 2 == 0) {
                spark = spark * 1103515245u + 12345u;
                uint8_t hue = (uint8_t)(spark >> 16);
                uint8_t r, g, b;
                if (spark & (1u << 8)) {
                    uint8_t v = (uint8_t)(48 + (hue % 208));
                    led_rgb(v, v, v);
                } else {
                    hsv_to_rgb(hue, &r, &g, &b);
                    led_rgb(r, g, b);
                }
            }
            break;
        case STATUS_LED_MODE_STATUS: {
            int clients = 0;
            wifi_sta_list_t sta;
            if (esp_wifi_ap_get_sta_list(&sta) == ESP_OK) {
                clients = sta.num;
            }
            if (!s_sd_ok) {
                led_rgb(255, 0, 0);
            } else if (clients > 0) {
                led_rgb(0, 180, 255);
            } else {
                led_rgb(0, 255, 0);
            }
            break;
        }
        default:
            if (tick % 20 == 0) {
                led_apply_solid(mode);
            }
            break;
        }
        tick++;
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

static void add_strip(int gpio)
{
    if (s_nstrips >= 2) {
        return;
    }
    led_strip_config_t strip_config = {
        .strip_gpio_num = gpio,
        .max_leds = 1,
    };
    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000,
        .flags.with_dma = false,
    };
    led_strip_handle_t strip = NULL;
    esp_err_t err = led_strip_new_rmt_device(&strip_config, &rmt_config, &strip);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "LED init failed on GPIO %d: %s", gpio, esp_err_to_name(err));
        return;
    }
    s_strips[s_nstrips++] = strip;
    ESP_LOGI(TAG, "RGB LED GPIO %d", gpio);
}

void status_led_init(uint8_t mode)
{
    add_strip(CONFIG_STATUS_LED_GPIO);
    /* Many S3 DevKits use 48; some revisions use 38. Drive both so the
     * LED is dim regardless of which pin is actually wired. */
    if (CONFIG_STATUS_LED_GPIO == 48) {
        add_strip(38);
    } else if (CONFIG_STATUS_LED_GPIO == 38) {
        add_strip(48);
    }
    if (s_nstrips == 0) {
        return;
    }
    s_ready = true;
    status_led_set_mode(mode);
    xTaskCreate(led_task, "status_led", 2048, NULL, 3, NULL);
    ESP_LOGI(TAG, "RGB LED brightness %d, mode %u",
             CONFIG_STATUS_LED_BRIGHTNESS, (unsigned)s_mode);
}

void status_led_set_mode(uint8_t mode)
{
    if (mode > STATUS_LED_MODE_MAX) {
        mode = STATUS_LED_MODE_DIM_GREEN;
    }
    s_mode = mode;
    if (s_ready) {
        uint8_t r, g, b;
        if (mode == STATUS_LED_MODE_OFF || led_solid_rgb(mode, &r, &g, &b)) {
            led_apply_solid(mode);
        }
    }
}

void status_led_set_sd_ok(bool ok)
{
    s_sd_ok = ok;
}

#else /* !CONFIG_STATUS_LED */

void status_led_init(uint8_t mode)
{
    (void)mode;
}

void status_led_set_mode(uint8_t mode)
{
    (void)mode;
}

void status_led_set_sd_ok(bool ok)
{
    (void)ok;
}

#endif /* CONFIG_STATUS_LED */
