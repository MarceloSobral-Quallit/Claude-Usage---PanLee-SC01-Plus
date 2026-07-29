#include "api.h"
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "API";

#define H5U "anthropic-ratelimit-unified-5h-utilization"
#define H5R "anthropic-ratelimit-unified-5h-reset"
#define H5S "anthropic-ratelimit-unified-5h-status"
#define D7U "anthropic-ratelimit-unified-7d-utilization"
#define D7R "anthropic-ratelimit-unified-7d-reset"
#define D7S "anthropic-ratelimit-unified-7d-status"
#define UST "anthropic-ratelimit-unified-status"
#define URS "anthropic-ratelimit-unified-reset"
#define URC "anthropic-ratelimit-unified-representative-claim"
#define UFB "anthropic-ratelimit-unified-fallback-percentage"
#define UOS "anthropic-ratelimit-unified-overage-status"
#define UOR "anthropic-ratelimit-unified-overage-disabled-reason"

typedef struct {
    char h5u[16], d7u[16], h5r[16], d7r[16], urs[16], ufb[16];
    char ust[24], h5s[24], d7s[24], urc[24], uos[24], uor[64];
} rate_limit_headers_t;

static esp_err_t rate_limit_header_handler(esp_http_client_event_t *evt)
{
    if (evt->event_id != HTTP_EVENT_ON_HEADER || evt->user_data == NULL) {
        return ESP_OK;
    }
    rate_limit_headers_t *h = (rate_limit_headers_t *)evt->user_data;

#define COPY_IF(name, field) \
    if (strcasecmp(evt->header_key, (name)) == 0) { \
        snprintf(h->field, sizeof(h->field), "%s", evt->header_value); \
        return ESP_OK; \
    }

    COPY_IF(H5U, h5u);
    COPY_IF(D7U, d7u);
    COPY_IF(H5R, h5r);
    COPY_IF(D7R, d7r);
    COPY_IF(URS, urs);
    COPY_IF(UFB, ufb);
    COPY_IF(UST, ust);
    COPY_IF(H5S, h5s);
    COPY_IF(D7S, d7s);
    COPY_IF(URC, urc);
    COPY_IF(UOS, uos);
    COPY_IF(UOR, uor);
#undef COPY_IF

    return ESP_OK;
}

static esp_http_client_handle_t open_messages_client(const char *token, void *user_data)
{
    esp_http_client_config_t config = {
        .url = MESSAGES_ENDPOINT,
        .method = HTTP_METHOD_POST,
        .timeout_ms = API_TIMEOUT_MS,
        .event_handler = rate_limit_header_handler,
        .user_data = user_data,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        return NULL;
    }

    char auth[300];
    snprintf(auth, sizeof(auth), "Bearer %s", token);
    esp_http_client_set_header(client, "Authorization", auth);
    esp_http_client_set_header(client, "anthropic-version", ANTHROPIC_VERSION);
    esp_http_client_set_header(client, "anthropic-beta", ANTHROPIC_BETA_HEADER);
    esp_http_client_set_header(client, "content-type", "application/json");
    esp_http_client_set_header(client, "User-Agent", CLAUDE_CODE_USER_AGENT);
    return client;
}

bool fetchUsage(const char *token, UsageData *out)
{
    memset(out, 0, sizeof(*out));
    rate_limit_headers_t headers = {0};

    esp_http_client_handle_t client = open_messages_client(token, &headers);
    if (client == NULL) {
        snprintf(out->error, sizeof(out->error), "https_init");
        return false;
    }

    char body[192];
    snprintf(body, sizeof(body),
             "{\"model\":\"%s\",\"max_tokens\":1,\"messages\":[{\"role\":\"user\",\"content\":\".\"}]}",
             PROBE_MODEL);
    esp_http_client_set_post_field(client, body, (int)strlen(body));

    ESP_LOGI(TAG, "POST %s", MESSAGES_ENDPOINT);
    esp_err_t err = esp_http_client_perform(client);
    if (err != ESP_OK) {
        snprintf(out->error, sizeof(out->error), "http_err_%d", err);
        esp_http_client_cleanup(client);
        return false;
    }

    int code = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);
    ESP_LOGI(TAG, "HTTP %d", code);

    if (headers.h5u[0] == '\0' && headers.d7u[0] == '\0') {
        if (code == 401) {
            snprintf(out->error, sizeof(out->error), "auth_failed");
        } else {
            snprintf(out->error, sizeof(out->error), "no_usage_h_%d", code);
        }
        return false;
    }

    out->h5 = strtof(headers.h5u, NULL) * 100.0f;
    out->d7 = strtof(headers.d7u, NULL) * 100.0f;
    out->h5ResetEpoch = (uint32_t)strtoul(headers.h5r, NULL, 10);
    out->d7ResetEpoch = (uint32_t)strtoul(headers.d7r, NULL, 10);
    out->unifiedResetEpoch = (uint32_t)strtoul(headers.urs, NULL, 10);
    out->fallbackPct = strtof(headers.ufb, NULL) * 100.0f;

    snprintf(out->statusOverall, sizeof(out->statusOverall), "%s", headers.ust);
    snprintf(out->status5h, sizeof(out->status5h), "%s", headers.h5s);
    snprintf(out->status7d, sizeof(out->status7d), "%s", headers.d7s);
    snprintf(out->repClaim, sizeof(out->repClaim), "%s", headers.urc);
    snprintf(out->overageStatus, sizeof(out->overageStatus), "%s", headers.uos);
    snprintf(out->overageReason, sizeof(out->overageReason), "%s", headers.uor);

    ESP_LOGI(TAG, "5h:%.0f%% (%s)  7d:%.0f%% (%s)  claim:%s  overall:%s",
             out->h5, out->status5h, out->d7, out->status7d, out->repClaim, out->statusOverall);

    out->ok = true;
    return true;
}

bool probeModel(const char *token, const char *modelId, ProbeResult *out)
{
    out->code = -1;
    out->ms = 0;

    esp_http_client_handle_t client = open_messages_client(token, NULL);
    if (client == NULL) {
        return false;
    }

    char body[192];
    snprintf(body, sizeof(body),
             "{\"model\":\"%s\",\"max_tokens\":1,\"messages\":[{\"role\":\"user\",\"content\":\".\"}]}",
             modelId);
    esp_http_client_set_post_field(client, body, (int)strlen(body));

    int64_t t0 = esp_timer_get_time();
    esp_err_t err = esp_http_client_perform(client);
    int64_t dt_ms = (esp_timer_get_time() - t0) / 1000;

    int code = (err == ESP_OK) ? esp_http_client_get_status_code(client) : (int)err;
    esp_http_client_cleanup(client);

    out->code = code;
    out->ms = (dt_ms > 65000) ? 65000 : (uint16_t)dt_ms;
    ESP_LOGI(TAG, "PROBE %s -> HTTP %d (%lldms)", modelId, code, dt_ms);
    return code == 200;
}
