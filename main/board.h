#pragma once

#include "esp_err.h"
#include "driver/spi_master.h"

#define BOARD_SPI_HOST SPI2_HOST

#ifdef __cplusplus
extern "C" {
#endif

/** Power rails, idle chip-selects. Call once at boot before SPI or the LED. */
void board_early_init(void);

/** Initialize SPI2 once (SD and, on T-Embed, the ST7789 share this bus). */
esp_err_t board_spi_bus_init(void);

/** Free SPI2 only when nothing else is using it (generic DevKit SD failure). */
void board_spi_bus_release_if_unused(void);

#ifdef __cplusplus
}
#endif
