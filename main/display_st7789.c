/*
 * ST7789 170x320 status display for LilyGO T-Embed CC1101.
 * Landscape 320x170. Shares SPI2 with the TF slot.
 */

#include "sdkconfig.h"

#if CONFIG_DISPLAY_ST7789

#include "display.h"
#include "display_font_ui.h"
#include "board.h"

#include "driver/gpio.h"
#include "esp_attr.h"
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
#define LCD_ROW_H           DISPLAY_FONT_H
#define LCD_GAP_X           0
#define LCD_GAP_Y           35
#define LCD_PCLK_HZ         (40 * 1000 * 1000)

static const char *TAG = "display";

static esp_lcd_panel_io_handle_t s_io;
static esp_lcd_panel_handle_t s_panel;
static uint16_t *s_line;
static bool s_initialized;
static volatile bool s_ui_busy;
static uint8_t s_enabled = 1;
static uint8_t s_contrast = DISPLAY_CONTRAST_MED;
static uint8_t s_invert;
static uint8_t s_rotate;
static uint8_t s_hw_on = 0xFF;
static uint8_t s_hw_inv = 0xFF;
static uint8_t s_hw_rot = 0xFF;
static char s_status_cache[6][32];
static int s_status_cache_n = -1;
static uint16_t s_status_cache_key;
static SemaphoreHandle_t s_mutex;
static SemaphoreHandle_t s_xfer_done;

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

static bool IRAM_ATTR on_color_done(esp_lcd_panel_io_handle_t io,
                                    esp_lcd_panel_io_event_data_t *edata,
                                    void *ctx)
{
    (void)io;
    (void)edata;
    (void)ctx;
    BaseType_t hp = pdFALSE;
    if (s_xfer_done) {
        xSemaphoreGiveFromISR(s_xfer_done, &hp);
    }
    return hp == pdTRUE;
}

static void line_wait(void)
{
    if (s_xfer_done) {
        xSemaphoreTake(s_xfer_done, pdMS_TO_TICKS(100));
    }
}

uint16_t display_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

static uint16_t mix565(uint16_t fg, uint16_t bg, unsigned t)
{
    if (t == 0) {
        return bg;
    }
    if (t >= 3) {
        return fg;
    }
    unsigned fr = (fg >> 11) & 0x1F;
    unsigned fg6 = (fg >> 5) & 0x3F;
    unsigned fb = fg & 0x1F;
    unsigned br = (bg >> 11) & 0x1F;
    unsigned bg6 = (bg >> 5) & 0x3F;
    unsigned bb = bg & 0x1F;
    unsigned r = (fr * t + br * (3 - t)) / 3;
    unsigned g = (fg6 * t + bg6 * (3 - t)) / 3;
    unsigned b = (fb * t + bb * (3 - t)) / 3;
    return (uint16_t)((r << 11) | (g << 5) | b);
}

static unsigned glyph_level(const uint8_t *glyph, int row, int col)
{
    unsigned bit = (unsigned)col * DISPLAY_FONT_BPP;
    unsigned byte = glyph[row * DISPLAY_FONT_ROW_BYTES + (bit / 8)];
    return (byte >> (bit % 8)) & 0x3u;
}

static void contrast_scale(uint8_t *r, uint8_t *g, uint8_t *b)
{
    unsigned n = 88;
    if (s_contrast == DISPLAY_CONTRAST_LOW) {
        n = 55;
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
    esp_lcd_panel_swap_xy(s_panel, true);
    if (s_rotate) {
        esp_lcd_panel_mirror(s_panel, true, false);
    } else {
        esp_lcd_panel_mirror(s_panel, false, true);
    }
    esp_lcd_panel_set_gap(s_panel, LCD_GAP_X, LCD_GAP_Y);
}

static void fill_span(int x, int y, int w, int h, uint16_t color)
{
    if (!s_panel || !s_line || w <= 0 || h <= 0) {
        return;
    }
    if (x < 0) {
        w += x;
        x = 0;
    }
    if (y < 0) {
        h += y;
        y = 0;
    }
    if (x + w > LCD_H_RES) {
        w = LCD_H_RES - x;
    }
    if (y + h > LCD_V_RES) {
        h = LCD_V_RES - y;
    }
    if (w <= 0 || h <= 0) {
        return;
    }
    int max_rows = LCD_ROW_H;
    for (int row = 0; row < h; row += max_rows) {
        int bh = h - row;
        if (bh > max_rows) {
            bh = max_rows;
        }
        int n = w * bh;
        line_wait();
        for (int i = 0; i < n; i++) {
            s_line[i] = color;
        }
        esp_lcd_panel_draw_bitmap(s_panel, x, y + row, x + w, y + row + bh, s_line);
    }
}

bool display_ui_busy(void)
{
    return s_ui_busy;
}

void display_ui_set_busy(bool busy)
{
    display_lock();
    s_ui_busy = busy;
    if (busy) {
        line_wait();
        if (s_xfer_done) {
            xSemaphoreGive(s_xfer_done);
        }
    }
    s_status_cache_n = -1;
    display_unlock();
}

void display_spi_claim(void)
{
    display_lock();
    line_wait();
}

void display_spi_release(void)
{
    if (s_xfer_done) {
        xSemaphoreGive(s_xfer_done);
    }
    display_unlock();
}

int display_font_w(void)
{
    return DISPLAY_FONT_W;
}

int display_font_h(void)
{
    return DISPLAY_FONT_H;
}

void display_clear(uint16_t color)
{
    display_lock();
    fill_span(0, 0, LCD_H_RES, LCD_V_RES, color);
    display_unlock();
}

void display_fill_rect(int x, int y, int w, int h, uint16_t color)
{
    display_lock();
    fill_span(x, y, w, h, color);
    display_unlock();
}

static void blit_text(int x, int y, const char *s, uint16_t fg, uint16_t bg)
{
    if (!s_panel || !s_line || !s || y < 0 || y + DISPLAY_FONT_H > LCD_V_RES) {
        return;
    }
    if (x < 0) {
        x = 0;
    }
    int max_w = LCD_H_RES - x;
    if (max_w <= 0) {
        return;
    }
    int nchars = 0;
    for (const char *p = s; *p && (nchars + 1) * DISPLAY_FONT_W <= max_w; p++) {
        nchars++;
    }
    if (nchars <= 0) {
        return;
    }
    int w = nchars * DISPLAY_FONT_W;
    uint16_t lut[4] = {
        bg,
        mix565(fg, bg, 1),
        mix565(fg, bg, 2),
        fg,
    };

    line_wait();
    for (int ci = 0; ci < nchars; ci++) {
        unsigned c = (unsigned char)s[ci];
        if (c < DISPLAY_FONT_FIRST || c >= DISPLAY_FONT_FIRST + DISPLAY_FONT_COUNT) {
            c = '?';
        }
        const uint8_t *glyph = display_font_bits[c - DISPLAY_FONT_FIRST];
        int gx = ci * DISPLAY_FONT_W;
        for (int row = 0; row < DISPLAY_FONT_H; row++) {
            uint16_t *dst = &s_line[row * w + gx];
            for (int col = 0; col < DISPLAY_FONT_W; col++) {
                dst[col] = lut[glyph_level(glyph, row, col)];
            }
        }
    }
    esp_lcd_panel_draw_bitmap(s_panel, x, y, x + w, y + DISPLAY_FONT_H, s_line);
}

void display_text(int x, int y, const char *s, uint16_t fg, uint16_t bg)
{
    display_lock();
    blit_text(x, y, s, fg, bg);
    display_unlock();
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
    if (s_hw_rot != s_rotate) {
        apply_rotation();
        s_hw_rot = s_rotate;
    }
    if (s_hw_inv != s_invert) {
        esp_lcd_panel_invert_color(s_panel, s_invert ? false : true);
        s_hw_inv = s_invert;
    }
    if (s_hw_on != s_enabled) {
        backlight_set(s_enabled != 0);
        s_hw_on = s_enabled;
        if (!s_enabled) {
            fill_span(0, 0, LCD_H_RES, LCD_V_RES, 0x0000);
        }
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

    if (!s_xfer_done) {
        s_xfer_done = xSemaphoreCreateBinary();
        if (s_xfer_done) {
            xSemaphoreGive(s_xfer_done);
        }
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
        .on_color_trans_done = on_color_done,
    };
    ret = esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)BOARD_SPI_HOST, &io_config, &s_io);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ST7789 panel IO failed: %s", esp_err_to_name(ret));
        return ret;
    }

    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = -1,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .data_endian = LCD_RGB_DATA_ENDIAN_LITTLE,
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
        fill_span(0, 0, LCD_H_RES, LCD_V_RES, display_rgb(0x12, 0x12, 0x22));
        s_initialized = true;
    }

    ESP_LOGI(TAG, "ST7789 ready (%dx%d landscape, CS=%d DC=%d BL=%d, font %dx%d)",
             LCD_H_RES, LCD_V_RES,
             CONFIG_DISPLAY_ST7789_CS_GPIO, CONFIG_DISPLAY_ST7789_DC_GPIO,
             CONFIG_DISPLAY_ST7789_BL_GPIO, DISPLAY_FONT_W, DISPLAY_FONT_H);
    return ESP_OK;
}

void display_status_update(const char *const *lines, unsigned count)
{
    display_lock();
    if (s_ui_busy || !s_enabled || !s_panel || !s_line) {
        display_unlock();
        return;
    }
    s_initialized = true;

    uint8_t fr = 0xf2, fg = 0xf2, fb = 0xf4;
    uint8_t hr = 0xff, hg = 0x7a, hb = 0x8a;
    uint8_t br = 0x12, bgc = 0x12, bb = 0x22;
    uint8_t hbr = 0x16, hbg = 0x21, hbb = 0x3e;
    contrast_scale(&fr, &fg, &fb);
    contrast_scale(&hr, &hg, &hb);
    uint16_t fg_c = display_rgb(fr, fg, fb);
    uint16_t accent = display_rgb(hr, hg, hb);
    uint16_t bg_c = display_rgb(br, bgc, bb);
    uint16_t head_bg = display_rgb(hbr, hbg, hbb);
    uint16_t mute = display_rgb(0x9a, 0x9a, 0xaa);
    if (s_invert) {
        uint16_t tmp = fg_c;
        fg_c = bg_c;
        bg_c = tmp;
    }

    unsigned n = count;
    if (n > 6) {
        n = 6;
    }
    uint16_t key = (uint16_t)(fg_c ^ bg_c ^ head_bg ^ accent ^ (s_contrast << 8) ^ s_invert);
    bool full = (s_status_cache_n != (int)n) || (s_status_cache_key != key);

    if (full) {
        fill_span(0, 0, LCD_H_RES, LCD_V_RES, bg_c);
        fill_span(0, 0, LCD_H_RES, DISPLAY_FONT_H + 4, head_bg);
        blit_text(8, 2, "Vibe Pocket", accent, head_bg);
    }

    int y = DISPLAY_FONT_H + 8;
    for (unsigned i = 0; i < n; i++) {
        const char *row = (lines && lines[i]) ? lines[i] : "";
        if (full || strncmp(s_status_cache[i], row, sizeof(s_status_cache[i]) - 1) != 0) {
            fill_span(0, y, LCD_H_RES, DISPLAY_FONT_H, bg_c);
            blit_text(8, y, row, fg_c, bg_c);
            strncpy(s_status_cache[i], row, sizeof(s_status_cache[i]) - 1);
            s_status_cache[i][sizeof(s_status_cache[i]) - 1] = '\0';
        }
        y += DISPLAY_FONT_H;
        if (y + DISPLAY_FONT_H > LCD_V_RES - 18) {
            break;
        }
    }
    if (full) {
        blit_text(8, LCD_V_RES - DISPLAY_FONT_H - 2, "Click knob: Settings", mute, bg_c);
    }
    s_status_cache_n = (int)n;
    s_status_cache_key = key;
    display_unlock();
}

#endif /* CONFIG_DISPLAY_ST7789 */
