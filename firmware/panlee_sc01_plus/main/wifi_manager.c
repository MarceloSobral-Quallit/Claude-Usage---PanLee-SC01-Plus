#include "wifi_manager.h"
#include "config.h"

#include <stdlib.h>
#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "lwip/inet.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "wifi_dev_seed.h"

static const char *TAG = "WIFI_MGR";

#define WIFI_CONNECTED_BIT BIT0

typedef struct {
    char ssid[WIFI_MANAGER_SSID_MAX];
    char pass[WIFI_MANAGER_PASS_MAX];
} saved_net_t;

static bool s_begun;
static EventGroupHandle_t s_events;
static bool s_connected;
static char s_ip[16] = "0.0.0.0";
static char s_ssid[WIFI_MANAGER_SSID_MAX] = "";
static saved_net_t s_nets[WIFI_MANAGER_MAX_NETWORKS];
static int s_count;

static void save_all(void)
{
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE_WIFI, NVS_READWRITE, &handle) != ESP_OK) {
        ESP_LOGW(TAG, "nvs_open (save) failed");
        return;
    }
    nvs_set_i32(handle, "count", s_count);
    for (int i = 0; i < s_count; i++) {
        char ks[16], kp[16];
        snprintf(ks, sizeof(ks), "s%d", i);
        snprintf(kp, sizeof(kp), "p%d", i);
        nvs_set_str(handle, ks, s_nets[i].ssid);
        nvs_set_str(handle, kp, s_nets[i].pass);
    }
    nvs_commit(handle);
    nvs_close(handle);
}

static void load_all(void)
{
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE_WIFI, NVS_READONLY, &handle) != ESP_OK) {
        s_count = 0;
        return;
    }
    int32_t count = 0;
    if (nvs_get_i32(handle, "count", &count) != ESP_OK) {
        count = 0;
    }
    if (count > WIFI_MANAGER_MAX_NETWORKS) {
        count = WIFI_MANAGER_MAX_NETWORKS;
    }
    s_count = (int)count;
    for (int i = 0; i < s_count; i++) {
        char ks[16], kp[16];
        snprintf(ks, sizeof(ks), "s%d", i);
        snprintf(kp, sizeof(kp), "p%d", i);
        size_t slen = sizeof(s_nets[i].ssid);
        size_t plen = sizeof(s_nets[i].pass);
        if (nvs_get_str(handle, ks, s_nets[i].ssid, &slen) != ESP_OK) {
            s_nets[i].ssid[0] = '\0';
        }
        if (nvs_get_str(handle, kp, s_nets[i].pass, &plen) != ESP_OK) {
            s_nets[i].pass[0] = '\0';
        }
    }
    nvs_close(handle);
}

static void promote(int idx)
{
    if (idx <= 0 || idx >= s_count) {
        return;
    }
    saved_net_t tmp = s_nets[idx];
    for (int i = idx; i > 0; i--) {
        s_nets[i] = s_nets[i - 1];
    }
    s_nets[0] = tmp;
    save_all();
}

static void add_network(const char *ssid, const char *pass)
{
    for (int i = 0; i < s_count; i++) {
        if (strcmp(s_nets[i].ssid, ssid) == 0) {
            snprintf(s_nets[i].pass, sizeof(s_nets[i].pass), "%s", pass);
            if (i > 0) {
                promote(i);
            } else {
                save_all();
            }
            return;
        }
    }
    int slots = s_count + 1;
    if (slots > WIFI_MANAGER_MAX_NETWORKS) {
        slots = WIFI_MANAGER_MAX_NETWORKS;
    }
    for (int i = slots - 1; i > 0; i--) {
        s_nets[i] = s_nets[i - 1];
    }
    snprintf(s_nets[0].ssid, sizeof(s_nets[0].ssid), "%s", ssid);
    snprintf(s_nets[0].pass, sizeof(s_nets[0].pass), "%s", pass);
    s_count = slots;
    save_all();
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void)arg;
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        s_connected = false;
        strcpy(s_ip, "0.0.0.0");
        xEventGroupClearBits(s_events, WIFI_CONNECTED_BIT);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        const ip_event_got_ip_t *event = (const ip_event_got_ip_t *)event_data;
        inet_ntoa_r(event->ip_info.ip, s_ip, sizeof(s_ip));
        s_connected = true;
        xEventGroupSetBits(s_events, WIFI_CONNECTED_BIT);
        ESP_LOGI(TAG, "connected; IP=%s", s_ip);
    }
}

void wifi_manager_begin(void)
{
    if (s_begun) {
        return;
    }
    s_begun = true;
    s_events = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    esp_err_t loop_err = esp_event_loop_create_default();
    if (loop_err != ESP_OK && loop_err != ESP_ERR_INVALID_STATE) {
        ESP_ERROR_CHECK(loop_err);
    }
    esp_netif_create_default_wifi_sta();

    const wifi_init_config_t init_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init_config));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    /* Sem isso, o modem sleep (power-save) do Wi-Fi corrompe handshakes TLS
     * consecutivos — confirmado em hardware: a 2a requisicao HTTPS de cada
     * ciclo (status.claude.com, logo apos api.anthropic.com) falhava com
     * "PK verify failed"/"signature verification failed" logo depois de um
     * log "wifi:m f null" (modem entrando em poupanca). O device fica ligado
     * na tomada, entao nao ha ganho real de energia em manter o power-save. */
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

    load_all();
    if (s_count == 0 && WIFI_DEV_SEED_SSID[0] != '\0') {
        ESP_LOGI(TAG, "nenhuma rede salva; pre-semeando '%s' de wifi_cred.txt (conveniencia de dev)",
                 WIFI_DEV_SEED_SSID);
        add_network(WIFI_DEV_SEED_SSID, WIFI_DEV_SEED_PASSWORD);
    }
    ESP_LOGI(TAG, "begin: %d redes salvas", s_count);
}

static bool connect_and_wait(const char *ssid, const char *pass, int timeout_ms)
{
    wifi_config_t config = {0};
    snprintf((char *)config.sta.ssid, sizeof(config.sta.ssid), "%s", ssid);
    snprintf((char *)config.sta.password, sizeof(config.sta.password), "%s", pass);
    config.sta.threshold.authmode = pass[0] == '\0' ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK;
    config.sta.pmf_cfg.capable = true;
    config.sta.pmf_cfg.required = false;

    xEventGroupClearBits(s_events, WIFI_CONNECTED_BIT);
    if (esp_wifi_set_config(WIFI_IF_STA, &config) != ESP_OK) {
        return false;
    }
    esp_wifi_disconnect();
    if (esp_wifi_connect() != ESP_OK) {
        return false;
    }

    const EventBits_t bits = xEventGroupWaitBits(s_events, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE,
                                                  pdMS_TO_TICKS(timeout_ms));
    if (bits & WIFI_CONNECTED_BIT) {
        snprintf(s_ssid, sizeof(s_ssid), "%s", ssid);
        return true;
    }
    esp_wifi_disconnect();
    return false;
}

bool wifi_manager_autoconnect(int timeout_ms)
{
    for (int i = 0; i < s_count; i++) {
        ESP_LOGI(TAG, "tentando '%s' (%d/%d)...", s_nets[i].ssid, i + 1, s_count);
        if (connect_and_wait(s_nets[i].ssid, s_nets[i].pass, timeout_ms)) {
            if (i > 0) {
                promote(i);
            }
            return true;
        }
    }
    ESP_LOGW(TAG, "nenhuma rede salva disponivel");
    return false;
}

bool wifi_manager_connect_to(const char *ssid, const char *pass, int timeout_ms)
{
    ESP_LOGI(TAG, "conectando a '%s'...", ssid);
    if (connect_and_wait(ssid, pass, timeout_ms)) {
        add_network(ssid, pass);
        return true;
    }
    ESP_LOGW(TAG, "falha ao conectar a '%s'", ssid);
    return false;
}

int wifi_manager_scan(wifi_scan_result_t *results, int max_results)
{
    const wifi_scan_config_t scan_config = {0};
    if (esp_wifi_scan_start(&scan_config, true) != ESP_OK) {
        return 0;
    }
    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);
    if (ap_count == 0) {
        return 0;
    }
    if (ap_count > (uint16_t)max_results) {
        ap_count = (uint16_t)max_results;
    }
    wifi_ap_record_t *records = calloc(ap_count, sizeof(wifi_ap_record_t));
    if (records == NULL) {
        return 0;
    }
    uint16_t got = ap_count;
    esp_wifi_scan_get_ap_records(&got, records);

    int count = got < (uint16_t)max_results ? got : max_results;
    for (int i = 0; i < count; i++) {
        snprintf(results[i].ssid, sizeof(results[i].ssid), "%s", (const char *)records[i].ssid);
        results[i].rssi = records[i].rssi;
        results[i].open = records[i].authmode == WIFI_AUTH_OPEN;
    }
    free(records);
    return count;
}

bool wifi_manager_is_connected(void)
{
    return s_connected;
}

const char *wifi_manager_get_ip(void)
{
    return s_ip;
}

const char *wifi_manager_get_ssid(void)
{
    return s_ssid;
}

int wifi_manager_saved_count(void)
{
    return s_count;
}

const char *wifi_manager_saved_ssid(int idx)
{
    if (idx < 0 || idx >= s_count) {
        return "";
    }
    return s_nets[idx].ssid;
}

void wifi_manager_forget(int idx)
{
    if (idx < 0 || idx >= s_count) {
        return;
    }
    for (int i = idx; i < s_count - 1; i++) {
        s_nets[i] = s_nets[i + 1];
    }
    s_count--;
    save_all();
}

void wifi_manager_forget_all(void)
{
    s_count = 0;
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE_WIFI, NVS_READWRITE, &handle) == ESP_OK) {
        nvs_erase_all(handle);
        nvs_commit(handle);
        nvs_close(handle);
    }
}

void wifi_manager_disconnect(void)
{
    esp_wifi_disconnect();
}
