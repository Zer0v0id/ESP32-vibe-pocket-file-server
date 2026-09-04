/*
 * ST7789 170x320 status display for LilyGO T-Embed CC1101.
 * Landscape 320x170. Shares SPI2 with the TF slot.
 */

#include "sdkconfig.h"

#if CONFIG_DISPLAY_ST7789

#include "display.h"
#include "display_font.h"
#include "board.h"

#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include <string.h>

#define LCD_H_RES           320
#define LCD_V_RES           170
#define LCD_ROW_H           16
#define LCD_GAP_X           0
#define LCD_GAP_Y           35
#define LCD_PCLK_HZ         (40 * 1000 * 1000)

static const char *TAG = "display";

static esp_lcd_panel_io_handle_t s_io;
static esp_lcd_panel_handle_t s_panel;
static uint16_t *s_line;
static bool s_initialized;
static uint8_t s_enabled = 1;
static uint8_t s_contrast = DISPLAY_CONTRAST_MED;
static uint8_t s_invert;
static uint8_t s_rotate;
static SemaphoreHandle_t s_mutex;

static void display_lock(void)
{
    if (!s_mutex) {
        s_mutex = xSemaphoreCreateMutex();
    }
    if (s_mutex) {
        xSemaphoreTake(s_mutex, pdMS_TO_TICKS(2000));
    }
}

static void display_unlock(void)
{
    if (s_mutex) {
        xSemaphoreGive(s_mutex);
    }
}

static inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b)
{
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

static void contrast_scale(uint8_t *r, uint8_t *g, uint8_t *b)
{
    unsigned n = 80;
    if (s_contrast == DISPLAY_CONTRAST_LOW) {
        n = 40;
    } else if (s_contrast == DISPLAY_CONTRAST_HIGH) {
        n = 100;
    }
    *r = (uint8_t)((*r * n) / 100);
    *g = (uint8_t)((*g * n) / 100);
    *b = (uint8_t)((*b * n) / 100);
}

static void backlight_set(bool on)
{
    gpio_set_level((gpio_num_t)CONFIG_DISPLAY_ST7789_BL_GPIO, on ? 1 : 0);
}

static void apply_rotation(void)
{
    /* LilyGO landscape begin(3): MADCTL 0xA0. Rotate 180 uses 0x60. */
    esp_lcd_panel_swap_xy(s_panel, true);
    if (s_rotate) {
        esp_lcd_panel_mirror(s_panel, true, false);
    } else {
        esp_lcd_panel_mirror(s_panel, false, true);
    }
    esp_lcd_panel_set_gap(s_panel, LCD_GAP_X, LCD_GAP_Y);
}

static void fill_screen(uint16_t color)
{
    if (!s_panel || !s_line) {
        return;
    }
    for (int i = 0; i < LCD_H_RES * LCD_ROW_H; i++) {
        s_line[i] = color;
    }
    for (int y = 0; y < LCD_V_RES; y += LCD_ROW_H) {
        int h = LCD_ROW_H;
        if (y + h > LCD_V_RES) {
            h = LCD_V_RES - y;
        }
        esp_lcd_panel_draw_bitmap(s_panel, 0, y, LCD_H_RES, y + h, s_line);
    }
}

static void draw_text_row(int y, const char *str, uint16_t fg, uint16_t bg)
{
    int n = LCD_H_RES * LCD_ROW_H;
    for (int i = 0; i < n; i++) {
        s_line[i] = bg;
    }
    if (!str) {
        esp_lcd_panel_draw_bitmap(s_panel, 0, y, LCD_H_RES, y + LCD_ROW_H, s_line);
        return;
    }
    int x = 4;
    while (*str && x + 12 <= LCD_H_RES) {
        unsigned c = (unsigned char)*str++;
        if (c < 32 || c > 126) {
            c = '?';
        }
        const uint8_t *glyph = display_font5x7[c - 32];
        for (int col = 0; col < 5; col++) {
            uint8_t bits = glyph[col];
            for (int row = 0; row < 7; row++) {
                if (bits & (1u << row)) {
                    int px = x + col * 2;
                    int py = row * 2 + 1;
                    for (int dy = 0; dy < 2; dy++) {
                        for (int dx = 0; dx < 2; dx++) {
                            s_line[(py + dy) * LCD_H_RES + (px + dx)] = fg;
                        }
                    }
                }
            }
        }
        x += 12;
    }
    esp_lcd_panel_draw_bitmap(s_panel, 0, y, LCD_H_RES, y + LCD_ROW_H, s_line);
}

void display_configure(uint8_t enabled, uint8_t contrast, uint8_t invert, uint8_t rotate180)
{
    s_enabled = enabled ? 1 : 0;
    s_contrast = contrast;
    s_invert = invert ? 1 : 0;
    s_rotate = rotate180 ? 1 : 0;
}

static void apply_panel_settings(void)
{
    if (!s_panel) {
        return;
    }
    apply_rotation();
    /* LilyGO TFT_INVERSION_ON; Settings invert flips that. */
    esp_lcd_panel_invert_color(s_panel, s_invert ? false : true);
    backlight_set(s_enabled != 0);
    if (!s_enabled) {
        fill_screen(0x0000);
    }
}

void display_apply(void)
{
    if (!s_panel) {
        (void)display_status_init();
        return;
    }
    display_lock();
    apply_panel_settings();
    s_initialized = s_enabled ? true : false;
    display_unlock();
}

int display_status_init(void)
{
    if (s_panel) {
        apply_panel_settings();
        return ESP_OK;
    }

    gpio_config_t bl = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << CONFIG_DISPLAY_ST7789_BL_GPIO,
    };
    gpio_config(&bl);
    backlight_set(false);

    esp_err_t ret = board_spi_bus_init();
    if (ret != ESP_OK) {
        return ret;
    }

    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = CONFIG_DISPLAY_ST7789_DC_GPIO,
        .cs_gpio_num = CONFIG_DISPLAY_ST7789_CS_GPIO,
        .pclk_hz = LCD_PCLK_HZ,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    ret = esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)BOARD_SPI_HOST, &io_config, &s_io);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ST7789 panel IO failed: %s", esp_err_to_name(ret));
        return ret;
    }

    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = -1,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    ret = esp_lcd_new_panel_st7789(s_io, &panel_config, &s_panel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ST7789 driver failed: %s", esp_err_to_name(ret));
        return ret;
    }

    s_line = heap_caps_malloc(LCD_H_RES * LCD_ROW_H * sizeof(uint16_t), MALLOC_CAP_DMA);
    if (!s_line) {
        ESP_LOGE(TAG, "ST7789 DMA buffer alloc failed");
        return ESP_ERR_NO_MEM;
    }

    ESP_ERROR_CHECK(esp_lcd_panel_reset(s_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(s_panel));
    apply_panel_settings();
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(s_panel, true));

    if (s_enabled) {
        fill_screen(rgb565(0x1a, 0x1a, 0x2e));
        s_initialized = true;
    }

    ESP_LOGI(TAG, "ST7789 ready (%dx%d landscape, CS=%d DC=%d BL=%d)",
             LCD_H_RES, LCD_V_RES,
             CONFIG_DISPLAY_ST7789_CS_GPIO, CONFIG_DISPLAY_ST7789_DC_GPIO,
             CONFIG_DISPLAY_ST7789_BL_GPIO);
    return ESP_OK;
}

void display_status_update(const char *const *lines, unsigned count)
{
    display_lock();
    if (!s_panel || !s_line || !s_enabled) {
        display_unlock();
        return;
    }
    s_initialized = true;

    uint8_t fr = 0xee, fg = 0xee, fb = 0xf0;
    uint8_t br = 0x1a, bgc = 0x1a, bb = 0x2e;
    contrast_scale(&fr, &fg, &fb);
    uint16_t fg_c = rgb565(fr, fg, fb);
    uint16_t bg_c = rgb565(br, bgc, bb);
    if (s_invert) {
        uint16_t tmp = fg_c;
        fg_c = bg_c;
        bg_c = tmp;
    }

    unsigned n = count;
    if (n > 8) {
        n = 8;
    }
    fill_screen(bg_c);
    for (unsigned i = 0; i < n; i++) {
        int y = 5 + (int)i * 20;
        if (y + LCD_ROW_H > LCD_V_RES) {
            break;
        }
        draw_text_row(y, (lines && lines[i]) ? lines[i] : "", fg_c, bg_c);
    }
    display_unlock();
}

#endif /* CONFIG_DISPLAY_ST7789 */
