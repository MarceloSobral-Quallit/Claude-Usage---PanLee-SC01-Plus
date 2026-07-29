#pragma once

#include <stddef.h>

#include "esp_err.h"

esp_err_t bsp_sdcard_mount(size_t *file_count);
