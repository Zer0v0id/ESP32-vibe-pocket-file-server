/*
 * Status display: no-op stub, or SSD1306 128x64 I2C driver when
 * CONFIG_DISPLAY_SSD1306 is enabled.
 */

#include "sdkconfig.h"
#include "display.h"
#include "esp_err.h"
#include "esp_log.h"

static const char *TAG = "display";

#if CONFIG_DISPLAY_SSD1306

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>
#include <stdio.h>

#define I2C_MASTER_FREQ_HZ      100000
#define I2C_MASTER_TIMEOUT_MS   1000
#define I2C_PROBE_TIMEOUT_MS    20

#define SSD1306_WIDTH           128
#if CONFIG_DISPLAY_SSD1306_128X32
#define SSD1306_HEIGHT          32
#define SSD1306_PAGES           4
#define SSD1306_MULTIPLEX       0x1F
#define SSD1306_COM_PINS_CFG    0x02
#else
#define SSD1306_HEIGHT          64
#define SSD1306_PAGES           8
#define SSD1306_MULTIPLEX       0x3F
#define SSD1306_COM_PINS_CFG    0x12
#endif

#define SSD1306_CMD_SET_MEM_ADDR_MODE   0x20
#define SSD1306_CMD_SET_COLUMN_ADDR     0x21
#define SSD1306_CMD_SET_PAGE_ADDR       0x22
#define SSD1306_CMD_SET_START_LINE      0x40
#define SSD1306_CMD_SET_CONTRAST        0x81
#define SSD1306_CMD_CHARGE_PUMP         0x8D
#define SSD1306_CMD_SEGMENT_REMAP       0xA1
#define SSD1306_CMD_SEGMENT_REMAP_0     0xA0
#define SSD1306_CMD_ENTIRE_FROM_RAM     0xA4
#define SSD1306_CMD_DISPLAY_NORMAL      0xA6
#define SSD1306_CMD_DISPLAY_INVERSE     0xA7
#define SSD1306_CMD_SET_MULTIPLEX       0xA8
#define SSD1306_CMD_DISPLAY_OFF         0xAE
#define SSD1306_CMD_DISPLAY_ON          0xAF
#define SSD1306_CMD_COM_SCAN_INC        0xC0
#define SSD1306_CMD_COM_SCAN_DEC        0xC8
#define SSD1306_CMD_SET_DISPLAY_OFFSET  0xD3
#define SSD1306_CMD_SET_DISPLAY_CLOCK   0xD5
#define SSD1306_CMD_SET_PRECHARGE       0xD9
#define SSD1306_CMD_SET_COM_PINS        0xDA
#define SSD1306_CMD_SET_VCOM_DETECT     0xDB

/* 5x7 font, ASCII 32-126 */
static const uint8_t font5x7[][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00}, /* space */
    {0x00, 0x00, 0x5F, 0x00, 0x00}, /* ! */
    {0x00, 0x07, 0x00, 0x07, 0x00}, /* " */
    {0x14, 0x7F, 0x14, 0x7F, 0x14}, /* # */
    {0x24, 0x2A, 0x7F, 0x2A, 0x12}, /* $ */
    {0x23, 0x13, 0x08, 0x64, 0x62}, /* % */
    {0x36, 0x49, 0x55, 0x22, 0x50}, /* & */
    {0x00, 0x05, 0x03, 0x00, 0x00}, /* ' */
    {0x00, 0x1C, 0x22, 0x41, 0x00}, /* ( */
    {0x00, 0x41, 0x22, 0x1C, 0x00}, /* ) */
    {0x14, 0x08, 0x3E, 0x08, 0x14}, /* * */
    {0x08, 0x08, 0x3E, 0x08, 0x08}, /* + */
    {0x00, 0x50, 0x30, 0x00, 0x00}, /* , */
    {0x08, 0x08, 0x08, 0x08, 0x08}, /* - */
    {0x00, 0x60, 0x60, 0x00, 0x00}, /* . */
    {0x20, 0x10, 0x08, 0x04, 0x02}, /* / */
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, /* 0 */
    {0x00, 0x42, 0x7F, 0x40, 0x00}, /* 1 */
    {0x42, 0x61, 0x51, 0x49, 0x46}, /* 2 */
    {0x21, 0x41, 0x45, 0x4B, 0x31}, /* 3 */
    {0x18, 0x14, 0x12, 0x7F, 0x10}, /* 4 */
    {0x27, 0x45, 0x45, 0x45, 0x39}, /* 5 */
    {0x3C, 0x4A, 0x49, 0x49, 0x30}, /* 6 */
    {0x01, 0x71, 0x09, 0x05, 0x03}, /* 7 */
    {0x36, 0x49, 0x49, 0x49, 0x36}, /* 8 */
    {0x06, 0x49, 0x49, 0x29, 0x1E}, /* 9 */
    {0x00, 0x36, 0x36, 0x00, 0x00}, /* : */
    {0x00, 0x56, 0x36, 0x00, 0x00}, /* ; */
    {0x08, 0x14, 0x22, 0x41, 0x00}, /* < */
    {0x14, 0x14, 0x14, 0x14, 0x14}, /* = */
    {0x00, 0x41, 0x22, 0x14, 0x08}, /* > */
    {0x02, 0x01, 0x51, 0x09, 0x06}, /* ? */
    {0x32, 0x49, 0x79, 0x41, 0x3E}, /* @ */
    {0x7E, 0x11, 0x11, 0x11, 0x7E}, /* A */
    {0x7F, 0x49, 0x49, 0x49, 0x36}, /* B */
    {0x3E, 0x41, 0x41, 0x41, 0x22}, /* C */
    {0x7F, 0x41, 0x41, 0x22, 0x1C}, /* D */
    {0x7F, 0x49, 0x49, 0x49, 0x41}, /* E */
    {0x7F, 0x09, 0x09, 0x09, 0x01}, /* F */
    {0x3E, 0x41, 0x49, 0x49, 0x7A}, /* G */
    {0x7F, 0x08, 0x08, 0x08, 0x7F}, /* H */
    {0x00, 0x41, 0x7F, 0x41, 0x00}, /* I */
    {0x20, 0x40, 0x41, 0x3F, 0x01}, /* J */
    {0x7F, 0x08, 0x14, 0x22, 0x41}, /* K */
    {0x7F, 0x40, 0x40, 0x40, 0x40}, /* L */
    {0x7F, 0x02, 0x0C, 0x02, 0x7F}, /* M */
    {0x7F, 0x04, 0x08, 0x10, 0x7F}, /* N */
    {0x3E, 0x41, 0x41, 0x41, 0x3E}, /* O */
    {0x7F, 0x09, 0x09, 0x09, 0x06}, /* P */
    {0x3E, 0x41, 0x51, 0x21, 0x5E}, /* Q */
    {0x7F, 0x09, 0x19, 0x29, 0x46}, /* R */
    {0x46, 0x49, 0x49, 0x49, 0x31}, /* S */
    {0x01, 0x01, 0x7F, 0x01, 0x01}, /* T */
    {0x3F, 0x40, 0x40, 0x40, 0x3F}, /* U */
    {0x1F, 0x20, 0x40, 0x20, 0x1F}, /* V */
    {0x3F, 0x40, 0x38, 0x40, 0x3F}, /* W */
    {0x63, 0x14, 0x08, 0x14, 0x63}, /* X */
    {0x07, 0x08, 0x70, 0x08, 0x07}, /* Y */
    {0x61, 0x51, 0x49, 0x45, 0x43}, /* Z */
    {0x00, 0x7F, 0x41, 0x41, 0x00}, /* [ */
    {0x02, 0x04, 0x08, 0x10, 0x20}, /* backslash */
    {0x00, 0x41, 0x41, 0x7F, 0x00}, /* ] */
    {0x04, 0x02, 0x01, 0x02, 0x04}, /* ^ */
    {0x40, 0x40, 0x40, 0x40, 0x40}, /* _ */
    {0x00, 0x01, 0x02, 0x04, 0x00}, /* ` */
    {0x20, 0x54, 0x54, 0x54, 0x78}, /* a */
    {0x7F, 0x48, 0x44, 0x44, 0x38}, /* b */
    {0x38, 0x44, 0x44, 0x44, 0x20}, /* c */
    {0x38, 0x44, 0x44, 0x48, 0x7F}, /* d */
    {0x38, 0x54, 0x54, 0x54, 0x18}, /* e */
    {0x08, 0x7E, 0x09, 0x01, 0x02}, /* f */
    {0x0C, 0x52, 0x52, 0x52, 0x3E}, /* g */
    {0x7F, 0x08, 0x04, 0x04, 0x78}, /* h */
    {0x00, 0x44, 0x7D, 0x40, 0x00}, /* i */
    {0x20, 0x40, 0x44, 0x3D, 0x00}, /* j */
    {0x7F, 0x10, 0x28, 0x44, 0x00}, /* k */
    {0x00, 0x41, 0x7F, 0x40, 0x00}, /* l */
    {0x7C, 0x04, 0x18, 0x04, 0x78}, /* m */
    {0x7C, 0x08, 0x04, 0x04, 0x78}, /* n */
    {0x38, 0x44, 0x44, 0x44, 0x38}, /* o */
    {0x7C, 0x14, 0x14, 0x14, 0x08}, /* p */
    {0x08, 0x14, 0x14, 0x18, 0x7C}, /* q */
    {0x7C, 0x08, 0x04, 0x04, 0x08}, /* r */
    {0x48, 0x54, 0x54, 0x54, 0x20}, /* s */
    {0x04, 0x3F, 0x44, 0x40, 0x20}, /* t */
    {0x3C, 0x40, 0x40, 0x20, 0x7C}, /* u */
    {0x1C, 0x20, 0x40, 0x20, 0x1C}, /* v */
    {0x3C, 0x40, 0x30, 0x40, 0x3C}, /* w */
    {0x44, 0x28, 0x10, 0x28, 0x44}, /* x */
    {0x0C, 0x50, 0x50, 0x50, 0x3C}, /* y */
    {0x44, 0x64, 0x54, 0x4C, 0x44}, /* z */
    {0x00, 0x08, 0x36, 0x41, 0x00}, /* { */
    {0x00, 0x00, 0x7F, 0x00, 0x00}, /* | */
    {0x00, 0x41, 0x36, 0x08, 0x00}, /* } */
    {0x08, 0x04, 0x08, 0x10, 0x08}, /* ~ */
};

static uint8_t s_framebuffer[SSD1306_WIDTH * SSD1306_PAGES];
static uint8_t s_tx_buf[1 + 128];
static i2c_master_bus_handle_t s_bus;
static i2c_master_dev_handle_t s_dev;
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

static uint8_t contrast_byte(uint8_t level)
{
    switch (level) {
    case DISPLAY_CONTRAST_LOW:
        return 0x20;
    case DISPLAY_CONTRAST_HIGH:
        return 0xCF;
    default:
        return 0x7F;
    }
}

void display_configure(uint8_t enabled, uint8_t contrast, uint8_t invert, uint8_t rotate180)
{
    s_enabled = enabled ? 1 : 0;
    s_contrast = (contrast <= DISPLAY_CONTRAST_HIGH) ? contrast : DISPLAY_CONTRAST_MED;
    s_invert = invert ? 1 : 0;
    s_rotate = rotate180 ? 1 : 0;
}

static void display_teardown(void)
{
    if (s_dev) {
        i2c_master_bus_rm_device(s_dev);
        s_dev = NULL;
    }
    if (s_bus) {
        i2c_del_master_bus(s_bus);
        s_bus = NULL;
    }
    s_initialized = false;
}

static esp_err_t ssd1306_write_cmd(uint8_t cmd)
{
    uint8_t data[2] = {0x00, cmd};
    return i2c_master_transmit(s_dev, data, sizeof(data), I2C_MASTER_TIMEOUT_MS);
}

static esp_err_t ssd1306_write_cmds(const uint8_t *cmds, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        esp_err_t ret = ssd1306_write_cmd(cmds[i]);
        if (ret != ESP_OK) {
            return ret;
        }
    }
    return ESP_OK;
}

static void draw_char(uint8_t x, uint8_t page, char c)
{
    if (c < 32 || c > 126) {
        c = ' ';
    }
    if (x + 5 >= SSD1306_WIDTH || page >= SSD1306_PAGES) {
        return;
    }

    const uint8_t *glyph = font5x7[c - 32];
    for (int i = 0; i < 5; i++) {
        s_framebuffer[page * SSD1306_WIDTH + x + i] = glyph[i];
    }
    if (x + 5 < SSD1306_WIDTH) {
        s_framebuffer[page * SSD1306_WIDTH + x + 5] = 0x00;
    }
}

static void draw_string(uint8_t x, uint8_t page, const char *str)
{
    uint8_t pos = x;
    while (*str && pos + 6 <= SSD1306_WIDTH) {
        draw_char(pos, page, *str);
        pos += 6;
        str++;
    }
}

static esp_err_t display_refresh(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    const uint8_t addr_cmds[] = {
        SSD1306_CMD_SET_COLUMN_ADDR, 0x00, 0x7F,
        SSD1306_CMD_SET_PAGE_ADDR, 0x00, (uint8_t)(SSD1306_PAGES - 1),
    };
    esp_err_t ret = ssd1306_write_cmds(addr_cmds, sizeof(addr_cmds));
    if (ret != ESP_OK) {
        return ret;
    }

    const size_t chunk = sizeof(s_tx_buf) - 1;
    for (size_t off = 0; off < sizeof(s_framebuffer); off += chunk) {
        size_t n = sizeof(s_framebuffer) - off;
        if (n > chunk) {
            n = chunk;
        }
        s_tx_buf[0] = 0x40;
        memcpy(s_tx_buf + 1, s_framebuffer + off, n);
        ret = i2c_master_transmit(s_dev, s_tx_buf, n + 1, I2C_MASTER_TIMEOUT_MS);
        if (ret != ESP_OK) {
            return ret;
        }
    }
    return ESP_OK;
}

static esp_err_t i2c_bus_start(gpio_num_t sda, gpio_num_t scl)
{
    display_teardown();

    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = sda,
        .scl_io_num = scl,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags = {
            .enable_internal_pullup = true,
        },
    };

    esp_err_t ret = i2c_new_master_bus(&bus_cfg, &s_bus);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C bus init failed (SDA=%d SCL=%d): %s",
                 (int)sda, (int)scl, esp_err_to_name(ret));
        s_bus = NULL;
        return ret;
    }
    return ESP_OK;
}

static bool probe_oled_addr(uint16_t *out_addr)
{
    static const uint16_t addrs[] = {
        CONFIG_DISPLAY_SSD1306_I2C_ADDR, 0x3C, 0x3D,
    };
    for (size_t i = 0; i < sizeof(addrs) / sizeof(addrs[0]); i++) {
        if (i2c_master_probe(s_bus, addrs[i], I2C_PROBE_TIMEOUT_MS) == ESP_OK) {
            *out_addr = addrs[i];
            return true;
        }
    }
    return false;
}

int display_status_init(void)
{
    gpio_num_t cfg_sda = (gpio_num_t)CONFIG_DISPLAY_SSD1306_SDA_GPIO;
    gpio_num_t cfg_scl = (gpio_num_t)CONFIG_DISPLAY_SSD1306_SCL_GPIO;
    /* Configured pins first, then a short list of common I2C GPIOs.
     * Never 38/48 (onboard WS2812) or SD/USB pins. */
    static const gpio_num_t scan_pins[] = {
        GPIO_NUM_NC, GPIO_NUM_NC, /* filled with configured SDA/SCL */
        GPIO_NUM_18, GPIO_NUM_17, GPIO_NUM_8, GPIO_NUM_9, GPIO_NUM_10,
    };
    gpio_num_t pins[sizeof(scan_pins) / sizeof(scan_pins[0])];
    memcpy(pins, scan_pins, sizeof(scan_pins));
    pins[0] = cfg_sda;
    pins[1] = cfg_scl;

    gpio_num_t sda = GPIO_NUM_NC;
    gpio_num_t scl = GPIO_NUM_NC;
    uint16_t addr = 0;
    bool found = false;

    ESP_LOGI(TAG, "Probing SSD1306 (prefer SDA=%d SCL=%d)", (int)cfg_sda, (int)cfg_scl);
    for (size_t i = 0; i < sizeof(pins) / sizeof(pins[0]) && !found; i++) {
        for (size_t j = 0; j < sizeof(pins) / sizeof(pins[0]) && !found; j++) {
            if (pins[i] == pins[j]) {
                continue;
            }
            if (i2c_bus_start(pins[i], pins[j]) != ESP_OK) {
                continue;
            }
            vTaskDelay(pdMS_TO_TICKS(5));
            if (probe_oled_addr(&addr)) {
                sda = pins[i];
                scl = pins[j];
                found = true;
                ESP_LOGI(TAG, "Found OLED at 0x%02X on SDA=%d SCL=%d", addr, (int)sda, (int)scl);
            }
        }
    }

    if (!found) {
        ESP_LOGW(TAG, "OLED not found on SDA=%d SCL=%d or nearby I2C pins (8/9/10/17/18).",
                 (int)cfg_sda, (int)cfg_scl);
        ESP_LOGW(TAG, "Need 4-pin I2C module: VCC=3.3V, GND, SDA, SCL. Not a 7-pin SPI OLED.");
        display_teardown();
        return ESP_ERR_NOT_FOUND;
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };
    esp_err_t ret = i2c_master_bus_add_device(s_bus, &dev_cfg, &s_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C device add failed: %s", esp_err_to_name(ret));
        display_teardown();
        return ret;
    }

    uint8_t init_cmds[] = {
        SSD1306_CMD_DISPLAY_OFF,
        SSD1306_CMD_SET_DISPLAY_CLOCK, 0x80,
        SSD1306_CMD_SET_MULTIPLEX, SSD1306_MULTIPLEX,
        SSD1306_CMD_SET_DISPLAY_OFFSET, 0x00,
        SSD1306_CMD_SET_START_LINE | 0x00,
        SSD1306_CMD_CHARGE_PUMP, 0x14,
        SSD1306_CMD_SET_MEM_ADDR_MODE, 0x00,
        s_rotate ? SSD1306_CMD_SEGMENT_REMAP_0 : SSD1306_CMD_SEGMENT_REMAP,
        s_rotate ? SSD1306_CMD_COM_SCAN_INC : SSD1306_CMD_COM_SCAN_DEC,
        SSD1306_CMD_SET_COM_PINS, SSD1306_COM_PINS_CFG,
        SSD1306_CMD_SET_CONTRAST, contrast_byte(s_contrast),
        SSD1306_CMD_SET_PRECHARGE, 0xF1,
        SSD1306_CMD_SET_VCOM_DETECT, 0x40,
        SSD1306_CMD_ENTIRE_FROM_RAM,
        s_invert ? SSD1306_CMD_DISPLAY_INVERSE : SSD1306_CMD_DISPLAY_NORMAL,
        s_enabled ? SSD1306_CMD_DISPLAY_ON : SSD1306_CMD_DISPLAY_OFF,
    };

    vTaskDelay(pdMS_TO_TICKS(50));
    ret = ssd1306_write_cmds(init_cmds, sizeof(init_cmds));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SSD1306 init failed: %s", esp_err_to_name(ret));
        display_teardown();
        return ret;
    }

    memset(s_framebuffer, 0, sizeof(s_framebuffer));
    s_initialized = s_enabled ? true : false;
    ESP_LOGI(TAG, "SSD1306 ready (I2C 0x%02X, SDA=%d, SCL=%d, contrast=%u invert=%u rot=%u%s)",
             addr, (int)sda, (int)scl, (unsigned)s_contrast, (unsigned)s_invert,
             (unsigned)s_rotate, s_enabled ? "" : " off");
    return ESP_OK;
}

void display_status_update(const char *const *lines, unsigned count)
{
    display_lock();
    if (!s_initialized) {
        display_unlock();
        return;
    }

    memset(s_framebuffer, 0, sizeof(s_framebuffer));
    unsigned n = count;
    if (n > SSD1306_PAGES) {
        n = SSD1306_PAGES;
    }
    for (unsigned i = 0; i < n; i++) {
        if (lines && lines[i] && lines[i][0]) {
            draw_string(0, (uint8_t)i, lines[i]);
        }
    }

    esp_err_t ret = display_refresh();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "OLED refresh failed: %s", esp_err_to_name(ret));
    }
    display_unlock();
}

void display_apply(void)
{
    if (!s_dev) {
        (void)display_status_init();
        return;
    }
    display_lock();
    uint8_t cmds[] = {
        s_rotate ? SSD1306_CMD_SEGMENT_REMAP_0 : SSD1306_CMD_SEGMENT_REMAP,
        s_rotate ? SSD1306_CMD_COM_SCAN_INC : SSD1306_CMD_COM_SCAN_DEC,
        SSD1306_CMD_SET_CONTRAST, contrast_byte(s_contrast),
        s_invert ? SSD1306_CMD_DISPLAY_INVERSE : SSD1306_CMD_DISPLAY_NORMAL,
        s_enabled ? SSD1306_CMD_DISPLAY_ON : SSD1306_CMD_DISPLAY_OFF,
    };
    esp_err_t ret = ssd1306_write_cmds(cmds, sizeof(cmds));
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "OLED apply failed: %s", esp_err_to_name(ret));
    }
    s_initialized = s_enabled ? true : false;
    display_unlock();
}

#else /* !CONFIG_DISPLAY_SSD1306 */

void display_configure(uint8_t enabled, uint8_t contrast, uint8_t invert, uint8_t rotate180)
{
    (void)enabled;
    (void)contrast;
    (void)invert;
    (void)rotate180;
}

void display_apply(void)
{
}

int display_status_init(void)
{
    ESP_LOGI(TAG, "OLED disabled (build without display)");
    return ESP_OK;
}

void display_status_update(const char *const *lines, unsigned count)
{
    (void)lines;
    (void)count;
}

#endif /* CONFIG_DISPLAY_SSD1306 */
