#pragma once

#include <stdarg.h>

#include "esp_err.h"

esp_err_t bsp_debug_log_init(const char *path);
void bsp_debug_log(const char *format, ...);
