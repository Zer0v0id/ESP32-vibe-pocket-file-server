#pragma once

#include "esp_err.h"
#include "driver/spi_master.h"

#define BOARD_SPI_HOST SPI2_HOST

#ifdef __cplusplus
extern "C" {
#endif

/** Power rails, idle chip-selects. Call once at boot before SPI or the LED. */
void board_early_init(void);

/**
 * Deselect every chip on the shared SPI bus (LCD, SD, CC1101, nRF24).
 * Call before talking to the TF slot. No-op on the generic DevKit.
 */
void board_spi_prepare_sd(void);

/** Initialize SPI2 once (SD and, on T-Embed, the ST7789 share this bus). */
esp_err_t board_spi_bus_init(void);

/** Free SPI2 only when nothing else is using it (generic DevKit SD failure). */
void board_spi_bus_release_if_unused(void);

/** Cut T-Embed PWR_EN and enter deep sleep. Wake by plugging USB or the power button. */
void board_power_off(void);

#ifdef __cplusplus
}
#endif
