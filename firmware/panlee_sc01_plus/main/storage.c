#include "storage.h"
#include "config.h"

#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "STORAGE";

#define HISTORY_MAGIC   0x43555348u /* "CUSH" */
#define HISTORY_VERSION 1u

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t count;      /* amostras validas em `samples` (0..HISTORY_MAX_SAMPLES) */
    float last_h5;
    history_day_heat_t heat[STORAGE_HEAT_DAYS];
    /* history_sample_t samples[HISTORY_MAX_SAMPLES] segue logo apos este header no arquivo */
} history_header_t;

bool storage_load_token_blob(EncryptedBlob *blob)
{
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) {
        return false;
    }
    size_t len = sizeof(*blob);
    esp_err_t err = nvs_get_blob(handle, "blob", blob, &len);
    nvs_close(handle);
    return err == ESP_OK && len == sizeof(*blob);
}

bool storage_save_token_blob(const EncryptedBlob *blob)
{
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle) != ESP_OK) {
        return false;
    }
    esp_err_t err = nvs_set_blob(handle, "blob", blob, sizeof(*blob));
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    return err == ESP_OK;
}

int storage_load_pin_attempts(void)
{
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) {
        return 0;
    }
    int32_t attempts = 0;
    nvs_get_i32(handle, "pinatt", &attempts);
    nvs_close(handle);
    return (int)attempts;
}

void storage_save_pin_attempts(int attempts)
{
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle) == ESP_OK) {
        nvs_set_i32(handle, "pinatt", attempts);
        nvs_commit(handle);
        nvs_close(handle);
    }
}

void storage_load_settings(device_settings_t *out)
{
    out->poll_sec = DEFAULT_POLL_SEC;
    out->slideshow_sec = 0;
    out->tz_offset_hours = 0;
    out->brightness_idx = 2;
    out->language = 0;
    out->heat_mode = 3;
    out->pin_required = 1;

    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) {
        return;
    }
    int32_t v;
    if (nvs_get_i32(handle, "poll", &v) == ESP_OK) {
        out->poll_sec = (uint16_t)v;
    }
    if (nvs_get_i32(handle, "slide", &v) == ESP_OK) {
        out->slideshow_sec = (uint8_t)v;
    }
    if (nvs_get_i32(handle, "tz", &v) == ESP_OK) {
        out->tz_offset_hours = (int8_t)v;
    }
    if (nvs_get_i32(handle, "bri", &v) == ESP_OK) {
        out->brightness_idx = (uint8_t)v;
    }
    if (nvs_get_i32(handle, "lang", &v) == ESP_OK) {
        out->language = (uint8_t)v;
    }
    if (nvs_get_i32(handle, "heatm", &v) == ESP_OK) {
        out->heat_mode = (uint8_t)v;
    }
    if (nvs_get_i32(handle, "pinreq", &v) == ESP_OK) {
        out->pin_required = (uint8_t)v;
    }
    nvs_close(handle);
}

void storage_save_settings(const device_settings_t *in)
{
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle) != ESP_OK) {
        return;
    }
    nvs_set_i32(handle, "poll", in->poll_sec);
    nvs_set_i32(handle, "slide", in->slideshow_sec);
    nvs_set_i32(handle, "tz", in->tz_offset_hours);
    nvs_set_i32(handle, "bri", in->brightness_idx);
    nvs_set_i32(handle, "lang", in->language);
    nvs_set_i32(handle, "heatm", in->heat_mode);
    nvs_set_i32(handle, "pinreq", in->pin_required);
    nvs_commit(handle);
    nvs_close(handle);
}

void storage_load_ntp_server(char *out, size_t out_size)
{
    snprintf(out, out_size, "%s", NTP_SERVER_1);

    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) {
        return;
    }
    size_t len = out_size;
    nvs_get_str(handle, "ntpsrv", out, &len);
    nvs_close(handle);
}

void storage_save_ntp_server(const char *server)
{
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle) != ESP_OK) {
        return;
    }
    nvs_set_str(handle, "ntpsrv", server);
    nvs_commit(handle);
    nvs_close(handle);
}

void storage_factory_reset(void)
{
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle) == ESP_OK) {
        nvs_erase_all(handle);
        nvs_commit(handle);
        nvs_close(handle);
    }
    if (nvs_open(NVS_NAMESPACE_WIFI, NVS_READWRITE, &handle) == ESP_OK) {
        nvs_erase_all(handle);
        nvs_commit(handle);
        nvs_close(handle);
    }
    remove(HISTORY_FILE_PATH);
    ESP_LOGW(TAG, "factory reset: NVS e HISTORY.DAT apagados");
}

bool storage_history_load(history_sample_t *samples, int max_samples, int *out_count,
                           history_day_heat_t heat[STORAGE_HEAT_DAYS], float *out_last_h5)
{
    FILE *file = fopen(HISTORY_FILE_PATH, "rb");
    if (file == NULL) {
        return false;
    }

    history_header_t header;
    bool ok = fread(&header, sizeof(header), 1, file) == 1 &&
              header.magic == HISTORY_MAGIC && header.version == HISTORY_VERSION;
    if (ok) {
        int count = (int)header.count;
        if (count > max_samples) {
            count = max_samples;
        }
        ok = fread(samples, sizeof(history_sample_t), (size_t)count, file) == (size_t)count;
        if (ok) {
            *out_count = count;
            memcpy(heat, header.heat, sizeof(header.heat));
            *out_last_h5 = header.last_h5;
        }
    }
    fclose(file);
    if (!ok) {
        ESP_LOGW(TAG, "HISTORY.DAT invalido/corrompido; iniciando historico vazio");
    }
    return ok;
}

bool storage_history_save(const history_sample_t *samples, int count,
                           const history_day_heat_t heat[STORAGE_HEAT_DAYS], float last_h5)
{
    FILE *file = fopen(HISTORY_FILE_PATH, "wb");
    if (file == NULL) {
        return false;
    }

    history_header_t header = {
        .magic = HISTORY_MAGIC,
        .version = HISTORY_VERSION,
        .count = (uint32_t)count,
        .last_h5 = last_h5,
    };
    memcpy(header.heat, heat, sizeof(header.heat));

    bool ok = fwrite(&header, sizeof(header), 1, file) == 1 &&
              fwrite(samples, sizeof(history_sample_t), (size_t)count, file) == (size_t)count;
    fclose(file);
    if (!ok) {
        ESP_LOGW(TAG, "falha ao gravar HISTORY.DAT");
    }
    return ok;
}
