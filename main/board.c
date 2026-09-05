#include "board.h"
#include "sdkconfig.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "board";
static bool s_spi_ready;

#if CONFIG_BOARD_T_EMBED_CC1101
/* LilyGO factory_test utilities.h — not the original T-Embed, not DevKit. */
#define BOARD_NRF24_CS_GPIO   44
#define BOARD_NRF24_CE_GPIO   43
#define BOARD_PN532_RST_GPIO  45
#endif

void board_spi_prepare_sd(void)
{
#if CONFIG_BOARD_T_EMBED_CC1101
    gpio_set_level((gpio_num_t)CONFIG_DISPLAY_ST7789_CS_GPIO, 1);
    gpio_set_level((gpio_num_t)CONFIG_SD_SPI_CS_GPIO, 1);
    gpio_set_level((gpio_num_t)CONFIG_BOARD_RADIO_CS_GPIO, 1);
    gpio_set_level((gpio_num_t)BOARD_NRF24_CS_GPIO, 1);
#endif
}

void board_early_init(void)
{
#if CONFIG_BOARD_T_EMBED_CC1101
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << CONFIG_BOARD_PWR_EN_GPIO) |
                        (1ULL << CONFIG_BOARD_RADIO_CS_GPIO) |
                        (1ULL << CONFIG_SD_SPI_CS_GPIO) |
                        (1ULL << CONFIG_DISPLAY_ST7789_CS_GPIO) |
                        (1ULL << CONFIG_DISPLAY_ST7789_BL_GPIO) |
                        (1ULL << BOARD_NRF24_CS_GPIO) |
                        (1ULL << BOARD_NRF24_CE_GPIO) |
                        (1ULL << BOARD_PN532_RST_GPIO),
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
    gpio_set_level((gpio_num_t)BOARD_NRF24_CS_GPIO, 1);
    gpio_set_level((gpio_num_t)BOARD_NRF24_CE_GPIO, 0);
    gpio_set_level((gpio_num_t)BOARD_PN532_RST_GPIO, 1);
    /* VCC3V3 (TF + CC1101 + LEDs) comes up after PWR_EN. */
    vTaskDelay(pdMS_TO_TICKS(150));
    ESP_LOGI(TAG, "T-Embed CC1101: PWR_EN GPIO%d high; CS LCD=%d SD=%d CC1101=%d nRF=%d idle",
             CONFIG_BOARD_PWR_EN_GPIO,
             CONFIG_DISPLAY_ST7789_CS_GPIO,
             CONFIG_SD_SPI_CS_GPIO,
             CONFIG_BOARD_RADIO_CS_GPIO,
             BOARD_NRF24_CS_GPIO);
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
#if CONFIG_BOARD_T_EMBED_CC1101
    /* Shared MOSI/MISO with CC1101/LCD; keep MISO from floating low. */
    gpio_set_pull_mode((gpio_num_t)CONFIG_SD_SPI_MISO_GPIO, GPIO_PULLUP_ONLY);
#endif
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

void board_power_off(void)
{
#if CONFIG_BOARD_T_EMBED_CC1101
    gpio_set_level((gpio_num_t)CONFIG_DISPLAY_ST7789_BL_GPIO, 0);
    gpio_set_level((gpio_num_t)CONFIG_BOARD_PWR_EN_GPIO, 0);
    ESP_LOGI(TAG, "PWR_EN low, entering deep sleep");
#endif
    esp_deep_sleep_start();
}
