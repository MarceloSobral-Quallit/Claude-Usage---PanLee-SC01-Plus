#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WIFI_MANAGER_MAX_NETWORKS 3
#define WIFI_MANAGER_SSID_MAX     33
#define WIFI_MANAGER_PASS_MAX     65

typedef struct {
    char ssid[WIFI_MANAGER_SSID_MAX];
    int rssi;
    bool open;
} wifi_scan_result_t;

/* Inicializa netif/event loop/driver Wi-Fi (uma vez) e carrega as redes
 * salvas da NVS (namespace "wifi", ate WIFI_MANAGER_MAX_NETWORKS). */
void wifi_manager_begin(void);

/* Tenta cada rede salva (indice 0 primeiro) ate uma conectar. */
bool wifi_manager_autoconnect(int timeout_ms);

/* Conecta a uma rede especifica; se conectar, salva/promove nas redes
 * salvas (mesma logica do WiFiManager original: promove para o indice 0,
 * mantendo no maximo WIFI_MANAGER_MAX_NETWORKS). */
bool wifi_manager_connect_to(const char *ssid, const char *pass, int timeout_ms);

/* Varre redes visiveis; retorna quantas foram escritas em results. */
int wifi_manager_scan(wifi_scan_result_t *results, int max_results);

bool wifi_manager_is_connected(void);
const char *wifi_manager_get_ip(void);
const char *wifi_manager_get_ssid(void);

int wifi_manager_saved_count(void);
const char *wifi_manager_saved_ssid(int idx);
void wifi_manager_forget(int idx);
void wifi_manager_forget_all(void);
void wifi_manager_disconnect(void);

#ifdef __cplusplus
}
#endif
