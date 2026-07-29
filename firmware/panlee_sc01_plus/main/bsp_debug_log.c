#include "bsp_debug_log.h"

#include <errno.h>
#include <stdio.h>
#include <stdarg.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "SC01_FILELOG";
static const char *s_path;
static SemaphoreHandle_t s_lock;

esp_err_t bsp_debug_log_init(const char *path)
{
    if (path == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_lock == NULL) {
        s_lock = xSemaphoreCreateMutex();
        if (s_lock == NULL) {
            return ESP_ERR_NO_MEM;
        }
    }
    s_path = path;
    return ESP_OK;
}

void bsp_debug_log(const char *format, ...)
{
    if (s_path == NULL || s_lock == NULL) {
        return;
    }

    char message[220];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    if (xSemaphoreTake(s_lock, pdMS_TO_TICKS(100)) != pdTRUE) {
        return;
    }
    FILE *file = fopen(s_path, "a");
    if (file != NULL) {
        fprintf(file, "[%lld ms] %s\n", esp_timer_get_time() / 1000, message);
        fclose(file);
    } else {
        ESP_LOGW(TAG, "Cannot append %s (errno=%d)", s_path, errno);
    }
    xSemaphoreGive(s_lock);
}
