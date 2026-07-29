#include "bsp_sdcard.h"

#include <dirent.h>
#include <stdio.h>

#include "board.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"

static const char *TAG = "SC01_SD";
static const char *MOUNT_POINT = "/sdcard";
static bool s_spi_initialized;

esp_err_t bsp_sdcard_mount(size_t *file_count)
{
    if (file_count == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *file_count = 0;

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    const spi_bus_config_t bus_config = {
        .mosi_io_num = BSP_SD_PIN_MOSI,
        .miso_io_num = BSP_SD_PIN_MISO,
        .sclk_io_num = BSP_SD_PIN_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096,
    };
    esp_err_t result = spi_bus_initialize(host.slot, &bus_config, SDSPI_DEFAULT_DMA);
    if (result == ESP_OK) {
        s_spi_initialized = true;
    } else if (result != ESP_ERR_INVALID_STATE) {
        return result;
    }

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = BSP_SD_PIN_CS;
    slot_config.host_id = host.slot;
    const esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 0,
        .disk_status_check_enable = false,
        .use_one_fat = false,
    };
    sdmmc_card_t *card = NULL;
    result = esp_vfs_fat_sdspi_mount(MOUNT_POINT, &host, &slot_config, &mount_config, &card);
    if (result != ESP_OK) {
        if (s_spi_initialized) {
            spi_bus_free(host.slot);
            s_spi_initialized = false;
        }
        return result;
    }

    ESP_LOGI(TAG, "microSD mounted: SPI CLK=%d MOSI=%d MISO=%d CS=%d",
             BSP_SD_PIN_CLK, BSP_SD_PIN_MOSI, BSP_SD_PIN_MISO, BSP_SD_PIN_CS);
    sdmmc_card_print_info(stdout, card);

    DIR *directory = opendir(MOUNT_POINT);
    if (directory == NULL) {
        return ESP_FAIL;
    }
    struct dirent *entry;
    while ((entry = readdir(directory)) != NULL) {
        ++*file_count;
        ESP_LOGI(TAG, "root entry: %s", entry->d_name);
    }
    closedir(directory);
    return ESP_OK;
}
