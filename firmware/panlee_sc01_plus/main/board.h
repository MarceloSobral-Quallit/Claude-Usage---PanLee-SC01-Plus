#pragma once

/*
 * Smart Panlee / Wireless-Tag SC01 Plus
 * Board: ZX3D50CE08S-V16-USRC
 * PCB:   240221
 *
 * LCD: ST7796UI, MCU8080 (i80) 8-bit
 * Touch: FT6336U/FT5x06, I2C endereco 0x38
 *
 * Pinagem e contrato de cor validados fisicamente em 29/07/2026 — ver
 * firmware/panlee_sc01_plus/REFERENCIA-HARDWARE-LVGL.md antes de alterar
 * qualquer valor abaixo. Nao alterar simultaneamente byte swap, ordem
 * RGB/BGR, inversao, espelhamento ou os pinos sem um build de teste isolado.
 */

#define BSP_BOARD_NAME              "ZX3D50CE08S-V16-USRC"

/* Orientacao de producao deste firmware: paisagem 480x320 (validada). */
#define BSP_LCD_LANDSCAPE           1

#if BSP_LCD_LANDSCAPE
#define BSP_LCD_H_RES               480
#define BSP_LCD_V_RES               320
#else
#define BSP_LCD_H_RES               320
#define BSP_LCD_V_RES               480
#endif
#define BSP_LCD_BITS_PER_PIXEL      16
#define BSP_LCD_DATA_WIDTH          8

/* Clock de bring-up validado. */
#define BSP_LCD_PIXEL_CLOCK_HZ      (10 * 1000 * 1000)
#define BSP_LCD_DRAW_LINES          20

/* LCD 8080 bus */
#define BSP_LCD_PIN_RST             4
#define BSP_LCD_PIN_DC              0
#define BSP_LCD_PIN_WR              47
#define BSP_LCD_PIN_TE              48
#define BSP_LCD_PIN_BL              45
#define BSP_LCD_PIN_CS              (-1)
#define BSP_LCD_PIN_RD              (-1)

#define BSP_LCD_PIN_D0              9
#define BSP_LCD_PIN_D1              46
#define BSP_LCD_PIN_D2              3
#define BSP_LCD_PIN_D3              8
#define BSP_LCD_PIN_D4              18
#define BSP_LCD_PIN_D5              17
#define BSP_LCD_PIN_D6              16
#define BSP_LCD_PIN_D7              15

/* Backlight: canal/timer LEDC usados pelo brilho configuravel (Ajustes). */
#define BSP_LCD_BL_LEDC_TIMER       LEDC_TIMER_0
#define BSP_LCD_BL_LEDC_CHANNEL     LEDC_CHANNEL_0
#define BSP_LCD_BL_LEDC_FREQ_HZ     5000
#define BSP_LCD_BL_LEDC_RES         LEDC_TIMER_8_BIT

/* Touch FT6336U */
#define BSP_TOUCH_PIN_SDA           6
#define BSP_TOUCH_PIN_SCL           5
#define BSP_TOUCH_PIN_INT           7
#define BSP_TOUCH_PIN_RST           4   /* compartilhado com o reset do LCD; nao controlar em separado */
#define BSP_TOUCH_I2C_PORT          I2C_NUM_0
#define BSP_TOUCH_I2C_ADDR          0x38

/* microSD SPI — usado para HISTORY.DAT (historico/heatmap) e log persistente. */
#define BSP_SD_PIN_CLK              39
#define BSP_SD_PIN_MOSI             40
#define BSP_SD_PIN_MISO             38
#define BSP_SD_PIN_CS               41
