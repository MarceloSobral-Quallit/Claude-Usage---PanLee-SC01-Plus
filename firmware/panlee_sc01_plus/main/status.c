#include "status.h"
#include "config.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"

static const char *TAG = "STATUS";

#define STATUS_BODY_MAX 8192

static void to_lower_inplace(char *s)
{
    for (; *s != '\0'; ++s) {
        *s = (char)tolower((unsigned char)*s);
    }
}

bool fetchModelStatus(ModelStatus *out)
{
    esp_http_client_config_t config = {
        .url = STATUS_ENDPOINT,
        .method = HTTP_METHOD_GET,
        .timeout_ms = API_TIMEOUT_MS,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .user_agent = "claude-usage-stick/1.0",
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGW(TAG, "https_init failed");
        return false;
    }

    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "open failed: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return false;
    }

    const int content_length = esp_http_client_fetch_headers(client);
    ESP_LOGI(TAG, "GET %s", STATUS_ENDPOINT);

    const int code = esp_http_client_get_status_code(client);
    ESP_LOGI(TAG, "HTTP %d", code);
    if (code != 200) {
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return false; /* mantem o ultimo estado conhecido */
    }
    if (content_length > STATUS_BODY_MAX - 1) {
        ESP_LOGW(TAG, "body too large (%d)", content_length);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return false;
    }

    static char body[STATUS_BODY_MAX];
    int total = 0;
    while (total < STATUS_BODY_MAX - 1) {
        const int r = esp_http_client_read(client, body + total, STATUS_BODY_MAX - 1 - total);
        if (r <= 0) {
            break;
        }
        total += r;
    }
    body[total] = '\0';
    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    to_lower_inplace(body);
    out->haikuUp = strstr(body, "haiku") == NULL;
    out->sonnetUp = strstr(body, "sonnet") == NULL;
    out->opusUp = strstr(body, "opus") == NULL;
    out->fableUp = strstr(body, "fable") == NULL;
    out->ok = true;

    ESP_LOGI(TAG, "haiku:%d sonnet:%d opus:%d fable:%d", out->haikuUp, out->sonnetUp, out->opusUp, out->fableUp);
    return true;
}
