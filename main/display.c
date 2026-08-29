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
#include <string.h>
#include <stdio.h>

#define I2C_MASTER_FREQ_HZ      400000
#define I2C_MASTER_TIMEOUT_MS   1000

#define SSD1306_WIDTH           128
#define SSD1306_HEIGHT          64
#define SSD1306_PAGES           8

#define SSD1306_CMD_SET_MEM_ADDR_MODE   0x20
#define SSD1306_CMD_SET_COLUMN_ADDR     0x21
#define SSD1306_CMD_SET_PAGE_ADDR       0x22
#define SSD1306_CMD_SET_START_LINE      0x40
#define SSD1306_CMD_SET_CONTRAST        0x81
#define SSD1306_CMD_CHARGE_PUMP         0x8D
#define SSD1306_CMD_SEGMENT_REMAP       0xA1
#define SSD1306_CMD_ENTIRE_FROM_RAM     0xA4
#define SSD1306_CMD_DISPLAY_NORMAL      0xA6
#define SSD1306_CMD_SET_MULTIPLEX       0xA8
#define SSD1306_CMD_DISPLAY_OFF         0xAE
#define SSD1306_CMD_DISPLAY_ON          0xAF
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
static uint8_t s_tx_buf[1 + sizeof(s_framebuffer)];
static i2c_master_bus_handle_t s_bus;
static i2c_master_dev_handle_t s_dev;
static bool s_initialized;

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
        SSD1306_CMD_SET_PAGE_ADDR, 0x00, 0x07,
    };
    esp_err_t ret = ssd1306_write_cmds(addr_cmds, sizeof(addr_cmds));
    if (ret != ESP_OK) {
        return ret;
    }

    s_tx_buf[0] = 0x40;
    memcpy(s_tx_buf + 1, s_framebuffer, sizeof(s_framebuffer));
    return i2c_master_transmit(s_dev, s_tx_buf, sizeof(s_tx_buf), I2C_MASTER_TIMEOUT_MS);
}

int display_status_init(void)
{
    const gpio_num_t sda = (gpio_num_t)CONFIG_DISPLAY_SSD1306_SDA_GPIO;
    const gpio_num_t scl = (gpio_num_t)CONFIG_DISPLAY_SSD1306_SCL_GPIO;
    const uint16_t addr = (uint16_t)CONFIG_DISPLAY_SSD1306_I2C_ADDR;

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
        ESP_LOGE(TAG, "I2C bus init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    vTaskDelay(pdMS_TO_TICKS(50));

    ret = i2c_master_probe(s_bus, addr, 100);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "No SSD1306 at 0x%02X (SDA=%d SCL=%d): %s",
                 addr, (int)sda, (int)scl, esp_err_to_name(ret));
        display_teardown();
        return ret;
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };
    ret = i2c_master_bus_add_device(s_bus, &dev_cfg, &s_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C device add failed: %s", esp_err_to_name(ret));
        display_teardown();
        return ret;
    }

    const uint8_t init_cmds[] = {
        SSD1306_CMD_DISPLAY_OFF,
        SSD1306_CMD_SET_DISPLAY_CLOCK, 0x80,
        SSD1306_CMD_SET_MULTIPLEX, 0x3F,
        SSD1306_CMD_SET_DISPLAY_OFFSET, 0x00,
        SSD1306_CMD_SET_START_LINE | 0x00,
        SSD1306_CMD_CHARGE_PUMP, 0x14,
        SSD1306_CMD_SET_MEM_ADDR_MODE, 0x00,
        SSD1306_CMD_SEGMENT_REMAP,
        SSD1306_CMD_COM_SCAN_DEC,
        SSD1306_CMD_SET_COM_PINS, 0x12,
        SSD1306_CMD_SET_CONTRAST, 0x7F,
        SSD1306_CMD_SET_PRECHARGE, 0xF1,
        SSD1306_CMD_SET_VCOM_DETECT, 0x40,
        SSD1306_CMD_ENTIRE_FROM_RAM,
        SSD1306_CMD_DISPLAY_NORMAL,
        SSD1306_CMD_DISPLAY_ON,
    };

    vTaskDelay(pdMS_TO_TICKS(50));
    ret = ssd1306_write_cmds(init_cmds, sizeof(init_cmds));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SSD1306 init failed: %s", esp_err_to_name(ret));
        display_teardown();
        return ret;
    }

    memset(s_framebuffer, 0, sizeof(s_framebuffer));
    s_initialized = true;
    ESP_LOGI(TAG, "SSD1306 ready (I2C 0x%02X, SDA=%d, SCL=%d)", addr, (int)sda, (int)scl);
    return ESP_OK;
}

void display_status_update(const char *ssid, const char *ip_str, bool sd_ok, const char *url)
{
    if (!s_initialized) {
        return;
    }

    memset(s_framebuffer, 0, sizeof(s_framebuffer));
    draw_string(0, 0, "Vibe Pocket");

    char line[32];
    snprintf(line, sizeof(line), "SSID: %.16s", ssid ? ssid : "N/A");
    draw_string(0, 2, line);

    snprintf(line, sizeof(line), "IP: %.16s", ip_str ? ip_str : "N/A");
    draw_string(0, 3, line);

    draw_string(0, 4, sd_ok ? "SD: OK" : "SD: NO CARD");

    if (url && url[0] != '\0') {
        const char *shown = url;
        if (strncmp(url, "http://", 7) == 0) {
            shown = url + 7;
        }
        snprintf(line, sizeof(line), "%.20s", shown);
        draw_string(0, 6, line);
    }

    display_refresh();
}

#else /* !CONFIG_DISPLAY_SSD1306 */

int display_status_init(void)
{
    ESP_LOGI(TAG, "OLED disabled (build without display)");
    return ESP_OK;
}

void display_status_update(const char *ssid, const char *ip_str, bool sd_ok, const char *url)
{
    (void)ssid;
    (void)ip_str;
    (void)sd_ok;
    (void)url;
}

#endif /* CONFIG_DISPLAY_SSD1306 */
