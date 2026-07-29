/**
 * lv_conf.h — config do LVGL 9.2 para o Claude Usage Stick (Panlee SC01 Plus).
 *
 * Adaptado do lv_conf.h do projeto original (claude-usage-stick-SVGL). A
 * unica mudanca funcional é remover CANVAS/CHART (nunca usados pela UI: o
 * medidor, o grafico de tendencia e o heatmap sao todos desenhados com
 * objetos/linhas simples, nao com lv_canvas/lv_chart).
 *
 * Requer CONFIG_LV_CONF_SKIP=y (ver sdkconfig.defaults) para que o
 * componente gerenciado lvgl/lvgl use este arquivo em vez da configuracao
 * via menuconfig.
 */
#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/*====================
   COLOR
 *====================*/
#define LV_COLOR_DEPTH 16
/* O contrato de cor (BGR + swap_color_bytes no i80) é resolvido no driver
   de display (bsp_display.c); o LVGL entrega RGB565 canonico sem swap. */
#define LV_COLOR_16_SWAP 0

/*=========================
   MEMORIA
   Pool interno do LVGL (objetos/estilos). O buffer de render do LVGL é
   parcial (20 linhas) e fica em RAM interna — ver
   REFERENCIA-HARDWARE-LVGL.md sobre por que este projeto NAO usa um
   buffer full-frame em PSRAM como o firmware original.
 *=========================*/
#define LV_USE_STDLIB_MALLOC   LV_STDLIB_BUILTIN
#define LV_USE_STDLIB_STRING   LV_STDLIB_BUILTIN
#define LV_USE_STDLIB_SPRINTF  LV_STDLIB_BUILTIN
#define LV_MEM_SIZE            (96 * 1024U)

/*====================
   HAL / SISTEMA
 *====================*/
#define LV_USE_OS LV_OS_NONE
/* tick vem de lv_tick_set_cb() em app_main.c */

/*====================
   RENDER
 *====================*/
#define LV_USE_DRAW_SW 1

/*====================
   FONTES (Montserrat usadas na UI)
 *====================*/
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_18 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_22 1
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_MONTSERRAT_40 1
#define LV_FONT_MONTSERRAT_48 1
#define LV_FONT_DEFAULT &lv_font_montserrat_14

/*====================
   WIDGETS usados
 *====================*/
#define LV_USE_LABEL        1
#define LV_USE_BUTTON       1
#define LV_USE_BUTTONMATRIX 1
#define LV_USE_BAR          1
#define LV_USE_LIST         1
#define LV_USE_TEXTAREA     1
#define LV_USE_KEYBOARD     1
#define LV_USE_IMAGE        1
#define LV_USE_LINE         1
#define LV_USE_ARC          1
#define LV_USE_SPINNER      1
#define LV_USE_TILEVIEW     1

#endif /*LV_CONF_H*/
