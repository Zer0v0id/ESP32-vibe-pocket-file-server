/*
 * Status display: SSD1306 OLED 128x64 I2C driver
 */

#include "display.h"
#include "esp_err.h"
#include "esp_log.h"
#include "driver/i2c.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "display";

/* I2C configuration for ESP32-S3 */
#define I2C_MASTER_SCL_IO           9       /* GPIO for I2C SCL */
#define I2C_MASTER_SDA_IO           8       /* GPIO for I2C SDA */
#define I2C_MASTER_NUM              I2C_NUM_0
#define I2C_MASTER_FREQ_HZ          400000  /* 400kHz */
#define I2C_MASTER_TX_BUF_DISABLE   0
#define I2C_MASTER_RX_BUF_DISABLE   0
#define I2C_MASTER_TIMEOUT_MS       1000

/* SSD1306 configuration */
#define SSD1306_I2C_ADDR            0x3C    /* Common I2C address for SSD1306 */
#define SSD1306_WIDTH               128
#define SSD1306_HEIGHT              64
#define SSD1306_PAGES               8       /* 64 / 8 */

/* SSD1306 commands */
#define SSD1306_CMD_DISPLAY_OFF         0xAE
#define SSD1306_CMD_DISPLAY_ON          0xAF
#define SSD1306_CMD_SET_CONTRAST        0x81
#define SSD1306_CMD_DISPLAY_ALL_ON      0xA5
#define SSD1306_CMD_DISPLAY_NORMAL      0xA6
#define SSD1306_CMD_DISPLAY_INVERSE     0xA7
#define SSD1306_CMD_SET_MULTIPLEX       0xA8
#define SSD1306_CMD_SET_DISPLAY_OFFSET  0xD3
#define SSD1306_CMD_SET_START_LINE      0x40
#define SSD1306_CMD_SEGMENT_REMAP       0xA1
#define SSD1306_CMD_COM_SCAN_DEC        0xC8
#define SSD1306_CMD_SET_COM_PINS        0xDA
#define SSD1306_CMD_SET_PRECHARGE       0xD9
#define SSD1306_CMD_SET_VCOM_DETECT     0xDB
#define SSD1306_CMD_CHARGE_PUMP         0x8D
#define SSD1306_CMD_MEM_ADDR_MODE       0x20
#define SSD1306_CMD_SET_COLUMN_ADDR     0x21
#define SSD1306_CMD_SET_PAGE_ADDR       0x22

/* Simple 5x7 font (ASCII 32-127) */
static const uint8_t font5x7[][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00}, // (space)
    {0x00, 0x00, 0x5F, 0x00, 0x00}, // !
    {0x00, 0x07, 0x00, 0x07, 0x00}, // "
    {0x14, 0x7F, 0x14, 0x7F, 0x14}, // #
    {0x24, 0x2A, 0x7F, 0x2A, 0x12}, // $
    {0x23, 0x13, 0x08, 0x64, 0x62}, // %
    {0x36, 0x49, 0x55, 0x22, 0x50}, // &
    {0x00, 0x05, 0x03, 0x00, 0x00}, // '
    {0x00, 0x1C, 0x22, 0x41, 0x00}, // (
    {0x00, 0x41, 0x22, 0x1C, 0x00}, // )
    {0x14, 0x08, 0x3E, 0x08, 0x14}, // *
    {0x08, 0x08, 0x3E, 0x08, 0x08}, // +
    {0x00, 0x50, 0x30, 0x00, 0x00}, // ,
    {0x08, 0x08, 0x08, 0x08, 0x08}, // -
    {0x00, 0x60, 0x60, 0x00, 0x00}, // .
    {0x20, 0x10, 0x08, 0x04, 0x02}, // /
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, // 0
    {0x00, 0x42, 0x7F, 0x40, 0x00}, // 1
    {0x42, 0x61, 0x51, 0x49, 0x46}, // 2
    {0x21, 0x41, 0x45, 0x4B, 0x31}, // 3
    {0x18, 0x14, 0x12, 0x7F, 0x10}, // 4
    {0x27, 0x45, 0x45, 0x45, 0x39}, // 5
    {0x3C, 0x4A, 0x49, 0x49, 0x30}, // 6
    {0x01, 0x71, 0x09, 0x05, 0x03}, // 7
    {0x36, 0x49, 0x49, 0x49, 0x36}, // 8
    {0x06, 0x49, 0x49, 0x29, 0x1E}, // 9
    {0x00, 0x36, 0x36, 0x00, 0x00}, // :
    {0x00, 0x56, 0x36, 0x00, 0x00}, // ;
    {0x08, 0x14, 0x22, 0x41, 0x00}, // <
    {0x14, 0x14, 0x14, 0x14, 0x14}, // =
    {0x00, 0x41, 0x22, 0x14, 0x08}, // >
    {0x02, 0x01, 0x51, 0x09, 0x06}, // ?
    {0x32, 0x49, 0x79, 0x41, 0x3E}, // @
    {0x7E, 0x11, 0x11, 0x11, 0x7E}, // A
    {0x7F, 0x49, 0x49, 0x49, 0x36}, // B
    {0x3E, 0x41, 0x41, 0x41, 0x22}, // C
    {0x7F, 0x41, 0x41, 0x22, 0x1C}, // D
    {0x7F, 0x49, 0x49, 0x49, 0x41}, // E
    {0x7F, 0x09, 0x09, 0x09, 0x01}, // F
    {0x3E, 0x41, 0x49, 0x49, 0x7A}, // G
    {0x7F, 0x08, 0x08, 0x08, 0x7F}, // H
    {0x00, 0x41, 0x7F, 0x41, 0x00}, // I
    {0x20, 0x40, 0x41, 0x3F, 0x01}, // J
    {0x7F, 0x08, 0x14, 0x22, 0x41}, // K
    {0x7F, 0x40, 0x40, 0x40, 0x40}, // L
    {0x7F, 0x02, 0x0C, 0x02, 0x7F}, // M
    {0x7F, 0x04, 0x08, 0x10, 0x7F}, // N
    {0x3E, 0x41, 0x41, 0x41, 0x3E}, // O
    {0x7F, 0x09, 0x09, 0x09, 0x06}, // P
    {0x3E, 0x41, 0x51, 0x21, 0x5E}, // Q
    {0x7F, 0x09, 0x19, 0x29, 0x46}, // R
    {0x46, 0x49, 0x49, 0x49, 0x31}, // S
    {0x01, 0x01, 0x7F, 0x01, 0x01}, // T
    {0x3F, 0x40, 0x40, 0x40, 0x3F}, // U
    {0x1F, 0x20, 0x40, 0x20, 0x1F}, // V
    {0x3F, 0x40, 0x38, 0x40, 0x3F}, // W
    {0x63, 0x14, 0x08, 0x14, 0x63}, // X
    {0x07, 0x08, 0x70, 0x08, 0x07}, // Y
    {0x61, 0x51, 0x49, 0x45, 0x43}, // Z
    {0x00, 0x7F, 0x41, 0x41, 0x00}, // [
    {0x02, 0x04, 0x08, 0x10, 0x20}, // backslash
    {0x00, 0x41, 0x41, 0x7F, 0x00}, // ]
    {0x04, 0x02, 0x01, 0x02, 0x04}, // ^
    {0x40, 0x40, 0x40, 0x40, 0x40}, // _
    {0x00, 0x01, 0x02, 0x04, 0x00}, // `
    {0x20, 0x54, 0x54, 0x54, 0x78}, // a
    {0x7F, 0x48, 0x44, 0x44, 0x38}, // b
    {0x38, 0x44, 0x44, 0x44, 0x20}, // c
    {0x38, 0x44, 0x44, 0x48, 0x7F}, // d
    {0x38, 0x54, 0x54, 0x54, 0x18}, // e
    {0x08, 0x7E, 0x09, 0x01, 0x02}, // f
    {0x0C, 0x52, 0x52, 0x52, 0x3E}, // g
    {0x7F, 0x08, 0x04, 0x04, 0x78}, // h
    {0x00, 0x44, 0x7D, 0x40, 0x00}, // i
    {0x20, 0x40, 0x44, 0x3D, 0x00}, // j
    {0x7F, 0x10, 0x28, 0x44, 0x00}, // k
    {0x00, 0x41, 0x7F, 0x40, 0x00}, // l
    {0x7C, 0x04, 0x18, 0x04, 0x78}, // m
    {0x7C, 0x08, 0x04, 0x04, 0x78}, // n
    {0x38, 0x44, 0x44, 0x44, 0x38}, // o
    {0x7C, 0x14, 0x14, 0x14, 0x08}, // p
    {0x08, 0x14, 0x14, 0x18, 0x7C}, // q
    {0x7C, 0x08, 0x04, 0x04, 0x08}, // r
    {0x48, 0x54, 0x54, 0x54, 0x20}, // s
    {0x04, 0x3F, 0x44, 0x40, 0x20}, // t
    {0x3C, 0x40, 0x40, 0x20, 0x7C}, // u
    {0x1C, 0x20, 0x40, 0x20, 0x1C}, // v
    {0x3C, 0x40, 0x30, 0x40, 0x3C}, // w
    {0x44, 0x28, 0x10, 0x28, 0x44}, // x
    {0x0C, 0x50, 0x50, 0x50, 0x3C}, // y
    {0x44, 0x64, 0x54, 0x4C, 0x44}, // z
    {0x00, 0x08, 0x36, 0x41, 0x00}, // {
    {0x00, 0x00, 0x7F, 0x00, 0x00}, // |
    {0x00, 0x41, 0x36, 0x08, 0x00}, // }
    {0x08, 0x04, 0x08, 0x10, 0x08}, // ~
};

static uint8_t framebuffer[SSD1306_WIDTH * SSD1306_PAGES];
static bool display_initialized = false;

/* Write command to SSD1306 */
static esp_err_t ssd1306_write_cmd(uint8_t cmd)
{
    uint8_t data[2] = {0x00, cmd};  /* 0x00 = command mode */
    return i2c_master_write_to_device(I2C_MASTER_NUM, SSD1306_I2C_ADDR, data, 2, 
                                      pdMS_TO_TICKS(I2C_MASTER_TIMEOUT_MS));
}

/* Write multiple commands to SSD1306 */
static esp_err_t ssd1306_write_cmds(const uint8_t *cmds, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        esp_err_t ret = ssd1306_write_cmd(cmds[i]);
        if (ret != ESP_OK) return ret;
    }
    return ESP_OK;
}

/* Write data (framebuffer) to SSD1306 */
static esp_err_t ssd1306_write_data(const uint8_t *data, size_t len)
{
    /* For large transfers, split into chunks with 0x40 data mode prefix */
    const size_t chunk_size = 128;
    for (size_t i = 0; i < len; i += chunk_size) {
        size_t chunk = (len - i) > chunk_size ? chunk_size : (len - i);
        uint8_t buf[chunk_size + 1];
        buf[0] = 0x40;  /* 0x40 = data mode */
        memcpy(buf + 1, data + i, chunk);
        esp_err_t ret = i2c_master_write_to_device(I2C_MASTER_NUM, SSD1306_I2C_ADDR, 
                                                    buf, chunk + 1, 
                                                    pdMS_TO_TICKS(I2C_MASTER_TIMEOUT_MS));
        if (ret != ESP_OK) return ret;
    }
    return ESP_OK;
}

/* Initialize I2C and SSD1306 */
int display_status_init(void)
{
    /* Configure I2C */
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    
    esp_err_t ret = i2c_param_config(I2C_MASTER_NUM, &conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C param config failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = i2c_driver_install(I2C_MASTER_NUM, conf.mode, 
                            I2C_MASTER_RX_BUF_DISABLE, 
                            I2C_MASTER_TX_BUF_DISABLE, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C driver install failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    /* Initialize SSD1306 - standard 128x64 initialization sequence */
    const uint8_t init_cmds[] = {
        SSD1306_CMD_DISPLAY_OFF,
        SSD1306_CMD_SET_MULTIPLEX, 0x3F,        /* 64 lines */
        SSD1306_CMD_SET_DISPLAY_OFFSET, 0x00,
        SSD1306_CMD_SET_START_LINE | 0x00,
        SSD1306_CMD_SEGMENT_REMAP,              /* Remap: column 127 -> SEG0 */
        SSD1306_CMD_COM_SCAN_DEC,               /* Scan from COM[N-1] to COM0 */
        SSD1306_CMD_SET_COM_PINS, 0x12,
        SSD1306_CMD_SET_CONTRAST, 0x7F,
        SSD1306_CMD_DISPLAY_NORMAL,
        SSD1306_CMD_DISPLAY_ALL_ON,             /* Resume to RAM content display */
        SSD1306_CMD_CHARGE_PUMP, 0x14,          /* Enable charge pump */
        SSD1306_CMD_MEM_ADDR_MODE, 0x00,        /* Horizontal addressing mode */
        SSD1306_CMD_DISPLAY_ON,
    };
    
    /* Small delay to ensure display power-up */
    vTaskDelay(pdMS_TO_TICKS(100));
    
    ret = ssd1306_write_cmds(init_cmds, sizeof(init_cmds));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SSD1306 init failed: %s", esp_err_to_name(ret));
        i2c_driver_delete(I2C_MASTER_NUM);
        return ret;
    }
    
    /* Clear framebuffer */
    memset(framebuffer, 0, sizeof(framebuffer));
    display_initialized = true;
    
    ESP_LOGI(TAG, "SSD1306 OLED display initialized (I2C: SDA=%d, SCL=%d)", 
             I2C_MASTER_SDA_IO, I2C_MASTER_SCL_IO);
    return ESP_OK;
}

/* Clear framebuffer */
static void clear_framebuffer(void)
{
    memset(framebuffer, 0, sizeof(framebuffer));
}

/* Draw a character at (x, page) in framebuffer */
static void draw_char(uint8_t x, uint8_t page, char c)
{
    if (c < 32 || c > 126) c = ' ';
    if (x + 5 >= SSD1306_WIDTH || page >= SSD1306_PAGES) return;
    
    const uint8_t *glyph = font5x7[c - 32];
    for (int i = 0; i < 5; i++) {
        framebuffer[page * SSD1306_WIDTH + x + i] = glyph[i];
    }
    /* Add 1 pixel spacing */
    if (x + 5 < SSD1306_WIDTH) {
        framebuffer[page * SSD1306_WIDTH + x + 5] = 0x00;
    }
}

/* Draw string at (x, page) in framebuffer */
static void draw_string(uint8_t x, uint8_t page, const char *str)
{
    uint8_t pos = x;
    while (*str && pos + 6 <= SSD1306_WIDTH) {
        draw_char(pos, page, *str);
        pos += 6;  /* 5 pixels + 1 spacing */
        str++;
    }
}

/* Send framebuffer to display */
static esp_err_t display_refresh(void)
{
    if (!display_initialized) return ESP_ERR_INVALID_STATE;
    
    /* Set column and page address range to full screen */
    uint8_t addr_cmds[] = {
        SSD1306_CMD_SET_COLUMN_ADDR, 0x00, 0x7F,  /* Columns 0-127 */
        SSD1306_CMD_SET_PAGE_ADDR, 0x00, 0x07,    /* Pages 0-7 */
    };
    
    esp_err_t ret = ssd1306_write_cmds(addr_cmds, sizeof(addr_cmds));
    if (ret != ESP_OK) return ret;
    
    return ssd1306_write_data(framebuffer, sizeof(framebuffer));
}

void display_status_update(const char *ssid, const char *ip_str, bool sd_ok, const char *url)
{
    if (!display_initialized) return;
    
    clear_framebuffer();
    
    /* Title */
    draw_string(0, 0, "Vibe Pocket");
    
    /* SSID */
    char line[32];
    snprintf(line, sizeof(line), "SSID: %.16s", ssid ? ssid : "N/A");
    draw_string(0, 2, line);
    
    /* IP address */
    snprintf(line, sizeof(line), "IP: %.16s", ip_str ? ip_str : "N/A");
    draw_string(0, 3, line);
    
    /* SD card status */
    draw_string(0, 4, sd_ok ? "SD: OK" : "SD: NO CARD");
    
    /* URL (truncated to fit) */
    if (url && strlen(url) > 0) {
        /* Extract just the IP from URL like "http://192.168.4.1" */
        const char *display_url = url;
        if (strncmp(url, "http://", 7) == 0) {
            display_url = url + 7;
        }
        snprintf(line, sizeof(line), "%.20s", display_url);
        draw_string(0, 6, line);
    }
    
    display_refresh();
}
