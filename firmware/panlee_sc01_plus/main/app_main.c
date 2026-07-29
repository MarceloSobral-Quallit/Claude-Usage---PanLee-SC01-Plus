#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "api.h"
#include "app_state.h"
#include "board.h"
#include "bsp_debug_log.h"
#include "bsp_display.h"
#include "bsp_sdcard.h"
#include "config.h"
#include "driver/i2c.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "nvs_flash.h"
#include "onboarding_server.h"
#include "status.h"
#include "time_sync.h"
#include "wifi_manager.h"

/*
 * Claude Usage Stick — Panlee SC01 Plus (ESP32-S3, ST7796UI i80, FT6336U)
 * Dashboard do rate-limit do Claude Code. Porte de
 * claude-usage-stick-SVGL-main/firmware/claude_stick/claude_stick.ino (LVGL
 * 9, Arduino/QSPI) para ESP-IDF + LVGL 9 sobre o painel i80/ST7796
 * validado desta placa. Ver REFERENCIA-HARDWARE-LVGL.md.
 */

static const char *TAG = "APP_MAIN";

#define LVGL_DRAW_LINES BSP_LCD_DRAW_LINES

/* ============================================================
 * Pipeline de display/touch (validado no bring-up desta placa)
 * ============================================================ */
static bool touch_read(uint16_t *x, uint16_t *y)
{
    uint8_t reg = 0x02;
    uint8_t points = 0;
    if (i2c_master_write_read_device(BSP_TOUCH_I2C_PORT, BSP_TOUCH_I2C_ADDR, &reg, 1, &points, 1,
                                      pdMS_TO_TICKS(30)) != ESP_OK ||
        (points & 0x07) == 0) {
        return false;
    }

    uint8_t data[4] = {0};
    reg = 0x03;
    if (i2c_master_write_read_device(BSP_TOUCH_I2C_PORT, BSP_TOUCH_I2C_ADDR, &reg, 1, data, sizeof(data),
                                      pdMS_TO_TICKS(30)) != ESP_OK) {
        return false;
    }
    const uint16_t raw_x = ((uint16_t)(data[0] & 0x0F) << 8) | data[1];
    const uint16_t raw_y = ((uint16_t)(data[2] & 0x0F) << 8) | data[3];

    /* Mapeamento de touch validado para a orientacao paisagem (480x320):
       ver REFERENCIA-HARDWARE-LVGL.md. Nao alterar sem nova medicao. */
    *x = raw_y;
    *y = BSP_LCD_V_RES - 1 - raw_x;
    return *x < BSP_LCD_H_RES && *y < BSP_LCD_V_RES;
}

static void touch_init(void)
{
    const i2c_config_t config = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = BSP_TOUCH_PIN_SDA,
        .scl_io_num = BSP_TOUCH_PIN_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000,
    };
    ESP_ERROR_CHECK(i2c_param_config(BSP_TOUCH_I2C_PORT, &config));
    ESP_ERROR_CHECK(i2c_driver_install(BSP_TOUCH_I2C_PORT, config.mode, 0, 0, 0));
}

static uint32_t lv_tick_cb(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    const int width = area->x2 - area->x1 + 1;
    const int height = area->y2 - area->y1 + 1;
    ESP_ERROR_CHECK(bsp_display_draw_bitmap_raw(area->x1, area->y1, width, height, (const uint16_t *)px_map));
    lv_disp_flush_ready(disp);
}

static void lvgl_touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;
    uint16_t x = 0;
    uint16_t y = 0;
    if (touch_read(&x, &y)) {
        data->state = LV_INDEV_STATE_PRESSED;
        data->point.x = x;
        data->point.y = y;
        g_lastTouchMs = now_ms(); /* pausa o slideshow enquanto ha interacao */
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

/* ============================================================
 * Refresh em background (task dedicada; ver api.c/status.c). Mantem a task
 * de LVGL livre para lv_timer_handler()/touch durante a chamada HTTPS
 * bloqueante (~1-2s) — diferenca deliberada em relacao ao firmware
 * original (Arduino de nucleo unico, sem tasks: la o loop() bloqueava).
 * ============================================================ */
static volatile bool s_refresh_apply_pending;
static volatile bool s_refresh_is_initial;
static volatile bool s_refresh_ok;
static volatile bool s_refresh_rebuild;

static void refresh_task_fn(void *arg)
{
    bool is_initial = (bool)(intptr_t)arg;
    bool ok;
    bool rebuild = false;

    if (is_initial) {
        ensure_time();
        ok = fetchUsage(g_token, &g_usage);
        if (ok) {
            fetchModelStatus(&g_status);
            g_lastOkMs = now_ms();
            g_lastFetchOk = true;
            hist_push(g_usage.h5, g_usage.d7);
            accumulate_heat(g_usage.h5);
            save_history();
            check_thresholds();
            probe_next_model();
        } else {
            g_lastFetchOk = false;
        }
    } else {
        if (!wifi_manager_is_connected()) {
            wifi_manager_autoconnect(WIFI_CONNECT_TIMEOUT_MS);
        }
        ensure_time();
        UsageData u = {0};
        ok = fetchUsage(g_token, &u);
        if (ok) {
            g_usage = u;
            g_lastOkMs = now_ms();
            g_lastFetchOk = true;
            hist_push(u.h5, u.d7);
            accumulate_heat(u.h5);
            save_history();
            check_thresholds();
            int moodBefore[NMODELS];
            for (int i = 0; i < NMODELS; i++) {
                moodBefore[i] = model_mood(i);
            }
            fetchModelStatus(&g_status);
            probe_next_model();
            for (int i = 0; i < NMODELS; i++) {
                if (moodBefore[i] != model_mood(i)) {
                    rebuild = true;
                }
            }
        } else {
            g_lastFetchOk = false;
        }
    }

    g_lastPollMs = now_ms();
    g_refreshing = false;
    s_refresh_ok = ok;
    s_refresh_is_initial = is_initial;
    s_refresh_rebuild = rebuild;
    s_refresh_apply_pending = true;
    vTaskDelete(NULL);
}

void refresh_begin(bool is_initial)
{
    if (g_refreshing) {
        return;
    }
    g_refreshing = true;
    if (xTaskCreate(refresh_task_fn, "refresh", 8192, (void *)(intptr_t)is_initial, 4, NULL) != pdPASS) {
        g_refreshing = false;
        ESP_LOGE(TAG, "falha ao criar task de refresh");
    }
}

/* ============================================================
 * setup / loop
 * ============================================================ */
static void boot_init(void)
{
    esp_err_t nvs_err = nvs_flash_init();
    if (nvs_err == ESP_ERR_NVS_NO_FREE_PAGES || nvs_err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs_err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvs_err);

    ESP_ERROR_CHECK(bsp_display_init());
    touch_init();
    wifi_manager_begin();

    size_t sd_files = 0;
    esp_err_t sd_result = bsp_sdcard_mount(&sd_files);
    if (sd_result == ESP_OK) {
        ESP_LOGI(TAG, "microSD montado; entradas na raiz=%u", (unsigned)sd_files);
        bsp_debug_log_init("/sdcard/CLAUDESK.LOG");
        bsp_debug_log("firmware " FW_VERSION " boot; microSD montado; entradas=%u", (unsigned)sd_files);
        load_history();
    } else {
        ESP_LOGW(TAG, "microSD indisponivel: %s (historico/heatmap ficam vazios ate haver cartao)",
                 esp_err_to_name(sd_result));
    }

    load_persisted();
    apply_brightness();

    lv_init();
    lv_tick_set_cb(lv_tick_cb);

    const size_t draw_pixels = BSP_LCD_H_RES * LVGL_DRAW_LINES;
    lv_color_t *draw_buf = heap_caps_malloc(draw_pixels * sizeof(lv_color_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    ESP_ERROR_CHECK(draw_buf == NULL ? ESP_ERR_NO_MEM : ESP_OK);

    lv_display_t *display = lv_display_create(BSP_LCD_H_RES, BSP_LCD_V_RES);
    lv_display_set_flush_cb(display, lvgl_flush_cb);
    lv_display_set_buffers(display, draw_buf, NULL, draw_pixels * sizeof(lv_color_t), LV_DISPLAY_RENDER_MODE_PARTIAL);

    lv_indev_t *touch_indev = lv_indev_create();
    lv_indev_set_type(touch_indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(touch_indev, lvgl_touch_read_cb);

    static const onboarding_server_callbacks_t onboarding_cb = {
        .is_token_configured = onboarding_is_token_configured,
        .on_token_submitted = onboarding_on_token_submitted,
        .get_token_error = onboarding_get_token_error,
        .get_h5_reset_epoch = onboarding_get_h5_reset_epoch,
        .on_tokens_pushed = onboarding_on_tokens_pushed,
    };
    onboarding_server_start(&onboarding_cb);

    if (g_hasToken) {
        /* Tenta WiFi cedo (em paralelo o usuario digita o PIN na tela, se
         * "Solicitar PIN no boot" estiver ligado). */
        bool wifi_ok = wifi_manager_autoconnect(WIFI_CONNECT_TIMEOUT_MS);
        if (!g_pinRequired && auto_unlock_with_fixed_pin()) {
            ESP_LOGI(TAG, "PIN no boot desligado nos Ajustes; desbloqueando automaticamente");
            request_state(wifi_ok ? ST_LOADING : ST_WIFI);
        } else {
            request_state(ST_PIN);
        }
    } else {
        g_onboarding = true;
        /* Se ja ha WiFi salvo (reboot no meio do onboarding), pula direto p/ o token. */
        request_state(wifi_manager_autoconnect(WIFI_CONNECT_TIMEOUT_MS) ? ST_TOKEN : ST_WIFI);
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "=== Claude Usage Stick (SC01 Plus) fw %s ===", FW_VERSION);
    boot_init();

    while (true) {
        lv_timer_handler();

        if (g_state == ST_TOKEN) {
            ui_token_tick();
            if (g_tokenGot) {
                g_tokenGot = false;
                request_state(ST_SETUP_PIN);
            }
        }

        if (g_dirty) {
            g_dirty = false;
            render_state();
            if (g_state == ST_LOADING) {
                lv_timer_handler();
                lv_refr_now(NULL);
                refresh_begin(true);
            }
        }

        if (g_state == ST_MAIN && !g_refreshing &&
            (g_wantRefresh || now_ms() - g_lastPollMs > (uint32_t)g_pollSec * 1000)) {
            g_wantRefresh = false;
            refresh_begin(false);
        }

        if (s_refresh_apply_pending) {
            s_refresh_apply_pending = false;
            if (s_refresh_is_initial) {
                request_state(s_refresh_ok ? ST_MAIN : ST_ERROR);
            } else if (s_refresh_rebuild) {
                request_state(ST_MAIN);
            } else {
                refresh_ui_values();
            }
        }

        if (g_state == ST_MAIN) {
            dashboard_animate(now_ms());
        }

        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
