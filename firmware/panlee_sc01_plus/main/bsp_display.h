#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t bsp_display_init(void);
esp_err_t bsp_display_set_backlight(bool on);

/* level: 0-255 (PWM via LEDC). Chama bsp_display_init() antes. */
esp_err_t bsp_display_set_brightness(uint8_t level);

esp_err_t bsp_display_fill_rgb565(uint16_t color);
esp_err_t bsp_display_fill_rect(int x, int y, int width, int height, uint16_t color);
esp_err_t bsp_display_draw_bitmap_raw(int x, int y, int width, int height, const uint16_t *pixels);

#ifdef __cplusplus
}
#endif
