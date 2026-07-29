#include "bsp_display.h"
#include "board.h"

#include <stddef.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_lcd_io_i80.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_st7796.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

/*
 * Contrato de cor validado nesta placa (ver REFERENCIA-HARDWARE-LVGL.md):
 *   - swap_color_bytes = 1 no barramento i80
 *   - rgb_ele_order = BGR
 *   - inversao do painel habilitada
 *   - espelhamento X habilitado (retrato) / swap_xy (paisagem)
 * Nao alterar nenhum destes parametros sem um build de teste isolado e nova
 * validacao visual com o padrao de cores.
 */

static const char *TAG = "SC01_LCD";

static esp_lcd_i80_bus_handle_t s_i80_bus;
static esp_lcd_panel_io_handle_t s_io;
static esp_lcd_panel_handle_t s_panel;
static SemaphoreHandle_t s_flush_done;
static uint16_t *s_draw_buf;
static bool s_backlight_ledc_ready;

static bool lcd_color_trans_done_cb(esp_lcd_panel_io_handle_t panel_io,
                                    esp_lcd_panel_io_event_data_t *edata,
                                    void *user_ctx)
{
    (void)panel_io;
    (void)edata;

    BaseType_t high_task_wakeup = pdFALSE;
    SemaphoreHandle_t sem = (SemaphoreHandle_t)user_ctx;
    xSemaphoreGiveFromISR(sem, &high_task_wakeup);
    return high_task_wakeup == pdTRUE;
}

static esp_err_t backlight_ledc_init(void)
{
    const ledc_timer_config_t timer_config = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = BSP_LCD_BL_LEDC_RES,
        .timer_num = BSP_LCD_BL_LEDC_TIMER,
        .freq_hz = BSP_LCD_BL_LEDC_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_RETURN_ON_ERROR(ledc_timer_config(&timer_config), TAG, "backlight LEDC timer config failed");

    const ledc_channel_config_t channel_config = {
        .gpio_num = BSP_LCD_PIN_BL,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = BSP_LCD_BL_LEDC_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = BSP_LCD_BL_LEDC_TIMER,
        .duty = 0,
        .hpoint = 0,
    };
    ESP_RETURN_ON_ERROR(ledc_channel_config(&channel_config), TAG, "backlight LEDC channel config failed");

    s_backlight_ledc_ready = true;
    return ESP_OK;
}

esp_err_t bsp_display_set_brightness(uint8_t level)
{
    ESP_RETURN_ON_FALSE(s_backlight_ledc_ready, ESP_ERR_INVALID_STATE, TAG, "backlight not initialized");
    ESP_RETURN_ON_ERROR(ledc_set_duty(LEDC_LOW_SPEED_MODE, BSP_LCD_BL_LEDC_CHANNEL, level), TAG,
                        "backlight duty set failed");
    ESP_RETURN_ON_ERROR(ledc_update_duty(LEDC_LOW_SPEED_MODE, BSP_LCD_BL_LEDC_CHANNEL), TAG,
                        "backlight duty update failed");
    return ESP_OK;
}

esp_err_t bsp_display_set_backlight(bool on)
{
    return bsp_display_set_brightness(on ? 255 : 0);
}

esp_err_t bsp_display_init(void)
{
    ESP_LOGI(TAG, "Board: %s", BSP_BOARD_NAME);
    ESP_LOGI(TAG, "ST7796 I80: D0..D7=%d,%d,%d,%d,%d,%d,%d,%d DC=%d WR=%d RST=%d BL=%d",
             BSP_LCD_PIN_D0, BSP_LCD_PIN_D1, BSP_LCD_PIN_D2, BSP_LCD_PIN_D3,
             BSP_LCD_PIN_D4, BSP_LCD_PIN_D5, BSP_LCD_PIN_D6, BSP_LCD_PIN_D7,
             BSP_LCD_PIN_DC, BSP_LCD_PIN_WR, BSP_LCD_PIN_RST, BSP_LCD_PIN_BL);

    ESP_RETURN_ON_ERROR(backlight_ledc_init(), TAG, "backlight init failed");
    ESP_RETURN_ON_ERROR(bsp_display_set_backlight(false), TAG, "unable to disable backlight");

    s_flush_done = xSemaphoreCreateBinary();
    ESP_RETURN_ON_FALSE(s_flush_done != NULL, ESP_ERR_NO_MEM, TAG, "flush semaphore allocation failed");

    const size_t draw_buf_bytes = BSP_LCD_H_RES * BSP_LCD_DRAW_LINES * sizeof(uint16_t);
    s_draw_buf = heap_caps_malloc(draw_buf_bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
    ESP_RETURN_ON_FALSE(s_draw_buf != NULL, ESP_ERR_NO_MEM, TAG, "DMA draw buffer allocation failed");

    const esp_lcd_i80_bus_config_t bus_config = {
        .dc_gpio_num = BSP_LCD_PIN_DC,
        .wr_gpio_num = BSP_LCD_PIN_WR,
        .clk_src = LCD_CLK_SRC_PLL160M,
        .data_gpio_nums = {
            BSP_LCD_PIN_D0,
            BSP_LCD_PIN_D1,
            BSP_LCD_PIN_D2,
            BSP_LCD_PIN_D3,
            BSP_LCD_PIN_D4,
            BSP_LCD_PIN_D5,
            BSP_LCD_PIN_D6,
            BSP_LCD_PIN_D7,
        },
        .bus_width = BSP_LCD_DATA_WIDTH,
        .max_transfer_bytes = draw_buf_bytes,
        .dma_burst_size = 64,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_i80_bus(&bus_config, &s_i80_bus), TAG, "I80 bus creation failed");

    esp_lcd_panel_io_i80_config_t io_config = ST7796_PANEL_IO_I80_CONFIG(
        BSP_LCD_PIN_CS,
        lcd_color_trans_done_cb,
        s_flush_done
    );
    io_config.pclk_hz = BSP_LCD_PIXEL_CLOCK_HZ;
    /* Contrato de cor validado: nao remover nem inverter este flag. */
    io_config.flags.swap_color_bytes = 1;

    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_i80(s_i80_bus, &io_config, &s_io), TAG, "panel IO creation failed");

    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = BSP_LCD_PIN_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
        .bits_per_pixel = BSP_LCD_BITS_PER_PIXEL,
    };

    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_st7796(s_io, &panel_config, &s_panel), TAG, "ST7796 driver creation failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(s_panel), TAG, "ST7796 reset failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(s_panel), TAG, "ST7796 init failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_invert_color(s_panel, true), TAG, "color inversion configuration failed");
#if BSP_LCD_LANDSCAPE
    ESP_RETURN_ON_ERROR(esp_lcd_panel_mirror(s_panel, false, false), TAG, "landscape mirror configuration failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_swap_xy(s_panel, true), TAG, "landscape XY swap failed");
#else
    ESP_RETURN_ON_ERROR(esp_lcd_panel_mirror(s_panel, true, false), TAG, "horizontal mirror configuration failed");
#endif
    ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(s_panel, true), TAG, "display ON command failed");

    ESP_LOGI(TAG, "LCD initialized at %d MHz, logical %dx%d, inversion enabled",
             BSP_LCD_PIXEL_CLOCK_HZ / 1000000, BSP_LCD_H_RES, BSP_LCD_V_RES);
    return ESP_OK;
}

esp_err_t bsp_display_fill_rgb565(uint16_t color)
{
    ESP_RETURN_ON_FALSE(s_panel != NULL && s_draw_buf != NULL && s_flush_done != NULL,
                        ESP_ERR_INVALID_STATE, TAG, "display not initialized");

    const size_t pixels_per_chunk = BSP_LCD_H_RES * BSP_LCD_DRAW_LINES;
    for (size_t i = 0; i < pixels_per_chunk; ++i) {
        s_draw_buf[i] = color;
    }

    for (int y = 0; y < BSP_LCD_V_RES; y += BSP_LCD_DRAW_LINES) {
        int lines = BSP_LCD_V_RES - y;
        if (lines > BSP_LCD_DRAW_LINES) {
            lines = BSP_LCD_DRAW_LINES;
        }

        (void)xSemaphoreTake(s_flush_done, 0);

        ESP_RETURN_ON_ERROR(
            esp_lcd_panel_draw_bitmap(s_panel, 0, y, BSP_LCD_H_RES, y + lines, s_draw_buf),
            TAG,
            "draw failed at y=%d",
            y
        );

        ESP_RETURN_ON_FALSE(
            xSemaphoreTake(s_flush_done, pdMS_TO_TICKS(1000)) == pdTRUE,
            ESP_ERR_TIMEOUT,
            TAG,
            "LCD DMA timeout at y=%d",
            y
        );
    }

    return ESP_OK;
}

esp_err_t bsp_display_fill_rect(int x, int y, int width, int height, uint16_t color)
{
    ESP_RETURN_ON_FALSE(s_panel != NULL && s_draw_buf != NULL && s_flush_done != NULL,
                        ESP_ERR_INVALID_STATE, TAG, "display not initialized");
    ESP_RETURN_ON_FALSE(x >= 0 && y >= 0 && width > 0 && height > 0 &&
                        x + width <= BSP_LCD_H_RES && y + height <= BSP_LCD_V_RES,
                        ESP_ERR_INVALID_ARG, TAG, "rectangle outside display");

    const size_t max_pixels = BSP_LCD_H_RES * BSP_LCD_DRAW_LINES;
    for (size_t i = 0; i < max_pixels; ++i) {
        s_draw_buf[i] = color;
    }

    for (int row = y; row < y + height; row += BSP_LCD_DRAW_LINES) {
        const int lines = (y + height - row) > BSP_LCD_DRAW_LINES ? BSP_LCD_DRAW_LINES : (y + height - row);
        (void)xSemaphoreTake(s_flush_done, 0);
        ESP_RETURN_ON_ERROR(esp_lcd_panel_draw_bitmap(s_panel, x, row, x + width, row + lines, s_draw_buf),
                            TAG, "rectangle draw failed");
        ESP_RETURN_ON_FALSE(xSemaphoreTake(s_flush_done, pdMS_TO_TICKS(1000)) == pdTRUE,
                            ESP_ERR_TIMEOUT, TAG, "LCD DMA timeout");
    }
    return ESP_OK;
}

esp_err_t bsp_display_draw_bitmap_raw(int x, int y, int width, int height, const uint16_t *pixels)
{
    ESP_RETURN_ON_FALSE(s_panel != NULL && s_draw_buf != NULL && s_flush_done != NULL && pixels != NULL,
                        ESP_ERR_INVALID_STATE, TAG, "display not initialized");
    ESP_RETURN_ON_FALSE(x >= 0 && y >= 0 && width > 0 && height > 0 && x + width <= BSP_LCD_H_RES && y + height <= BSP_LCD_V_RES,
                        ESP_ERR_INVALID_ARG, TAG, "invalid bitmap area");
    const int max_lines = BSP_LCD_DRAW_LINES;
    for (int row = 0; row < height; row += max_lines) {
        const int lines = (height - row) > max_lines ? max_lines : (height - row);
        const size_t count = (size_t)width * lines;
        for (size_t i = 0; i < count; ++i) {
            s_draw_buf[i] = pixels[(size_t)row * width + i];
        }
        (void)xSemaphoreTake(s_flush_done, 0);
        ESP_RETURN_ON_ERROR(esp_lcd_panel_draw_bitmap(s_panel, x, y + row, x + width, y + row + lines, s_draw_buf),
                            TAG, "raw rectangle draw failed");
        ESP_RETURN_ON_FALSE(xSemaphoreTake(s_flush_done, pdMS_TO_TICKS(1000)) == pdTRUE,
                            ESP_ERR_TIMEOUT, TAG, "LCD DMA timeout");
    }
    return ESP_OK;
}
