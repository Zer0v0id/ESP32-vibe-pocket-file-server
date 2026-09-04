#include "board.h"
#include "sdkconfig.h"

#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "board";
static bool s_spi_ready;

void board_early_init(void)
{
#if CONFIG_BOARD_T_EMBED_CC1101
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << CONFIG_BOARD_PWR_EN_GPIO) |
                        (1ULL << CONFIG_BOARD_RADIO_CS_GPIO) |
                        (1ULL << CONFIG_SD_SPI_CS_GPIO) |
                        (1ULL << CONFIG_DISPLAY_ST7789_CS_GPIO) |
                        (1ULL << CONFIG_DISPLAY_ST7789_BL_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io);
    gpio_set_level((gpio_num_t)CONFIG_BOARD_PWR_EN_GPIO, 1);
    gpio_set_level((gpio_num_t)CONFIG_BOARD_RADIO_CS_GPIO, 1);
    gpio_set_level((gpio_num_t)CONFIG_SD_SPI_CS_GPIO, 1);
    gpio_set_level((gpio_num_t)CONFIG_DISPLAY_ST7789_CS_GPIO, 1);
    gpio_set_level((gpio_num_t)CONFIG_DISPLAY_ST7789_BL_GPIO, 0);
    ESP_LOGI(TAG, "T-Embed CC1101: PWR_EN GPIO%d high, CC1101 CS idle",
             CONFIG_BOARD_PWR_EN_GPIO);
#endif
}

esp_err_t board_spi_bus_init(void)
{
    if (s_spi_ready) {
        return ESP_OK;
    }
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = CONFIG_SD_SPI_MOSI_GPIO,
        .miso_io_num = CONFIG_SD_SPI_MISO_GPIO,
        .sclk_io_num = CONFIG_SD_SPI_SCK_GPIO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
#if CONFIG_DISPLAY_ST7789
        .max_transfer_sz = 320 * 40 * 2,
#else
        .max_transfer_sz = 4000,
#endif
    };
    esp_err_t ret = spi_bus_initialize(BOARD_SPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    if (ret == ESP_ERR_INVALID_STATE) {
        ret = ESP_OK;
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI bus init failed: %s", esp_err_to_name(ret));
        return ret;
    }
    s_spi_ready = true;
    ESP_LOGI(TAG, "SPI2 MOSI=%d MISO=%d SCK=%d",
             CONFIG_SD_SPI_MOSI_GPIO, CONFIG_SD_SPI_MISO_GPIO, CONFIG_SD_SPI_SCK_GPIO);
    return ESP_OK;
}

void board_spi_bus_release_if_unused(void)
{
#if !CONFIG_DISPLAY_ST7789
    if (s_spi_ready) {
        spi_bus_free(BOARD_SPI_HOST);
        s_spi_ready = false;
    }
#endif
}
