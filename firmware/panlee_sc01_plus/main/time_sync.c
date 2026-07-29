#include "time_sync.h"
#include "app_state.h"
#include "config.h"

#include "esp_log.h"
#include "esp_netif_sntp.h"

static const char *TAG = "TIME_SYNC";
static bool s_started;

void time_sync_start(void)
{
    if (s_started) {
        return;
    }
    s_started = true;

    /* g_ntpServer vem do NVS (editavel em Ajustes -> Servidor NTP); default
     * de fabrica e NTP_SERVER_1 (config.h). NTP_SERVER_2 e fixo, so como
     * fallback secundario. */
    const char *primary = g_ntpServer[0] ? g_ntpServer : NTP_SERVER_1;
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG(primary);
    config.num_of_servers = 2;
    config.servers[1] = NTP_SERVER_2;
    esp_err_t err = esp_netif_sntp_init(&config);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "esp_netif_sntp_init failed: %s", esp_err_to_name(err));
        return;
    }
    ESP_LOGI(TAG, "SNTP iniciado (%s, %s)", primary, NTP_SERVER_2);
}

bool time_sync_is_synced(void)
{
    time_t now = time(NULL);
    struct tm timeinfo;
    gmtime_r(&now, &timeinfo);
    return (timeinfo.tm_year + 1900) >= 2024;
}

time_t time_sync_now_utc(void)
{
    return time_sync_is_synced() ? time(NULL) : 0;
}
