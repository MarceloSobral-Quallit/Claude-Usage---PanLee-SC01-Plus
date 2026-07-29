#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "crypto.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Token cifrado + PIN (NVS, namespace "claude") ---- */
bool storage_load_token_blob(EncryptedBlob *blob);
bool storage_save_token_blob(const EncryptedBlob *blob);

int storage_load_pin_attempts(void);
void storage_save_pin_attempts(int attempts);

/* ---- Configuracoes (NVS, namespace "claude") ---- */
typedef struct {
    uint16_t poll_sec;          /* intervalo de atualizacao (30-300s) */
    uint8_t slideshow_sec;      /* 0=off, senao 5/10/15/30 */
    int8_t tz_offset_hours;     /* GMT+-N */
    uint8_t brightness_idx;     /* 0=baixo 1=medio 2=alto */
    uint8_t language;           /* 0=pt-BR 1=en */
    uint8_t heat_mode;          /* 0=hoje 1=7d 2=30d 3=tudo (filtro do heatmap) */
    uint8_t pin_required;       /* 1=pede PIN no boot (default) 0=usa FIXED_PIN automaticamente */
} device_settings_t;

/* Preenche com os defaults de config.h se ainda nao houver nada salvo. */
void storage_load_settings(device_settings_t *out);
void storage_save_settings(const device_settings_t *in);

/* Reset de fabrica: apaga NVS ("claude" + "wifi") e remove HISTORY.DAT do SD. */
void storage_factory_reset(void);

/* ---- Servidor NTP primario (editavel em Ajustes; NVS namespace "claude") ----
 * String separada (nao faz parte de device_settings_t, que so tem campos
 * fixos pequenos). Se nunca foi salvo, preenche "out" com NTP_SERVER_1
 * (config.h). Aplicado somente no PROXIMO boot (time_sync_start() so roda
 * uma vez, ver app_main.c). */
void storage_load_ntp_server(char *out, size_t out_size);
void storage_save_ntp_server(const char *server);

/* ---- Historico/heatmap (arquivo unico no microSD, ver config.h) ---- */
typedef struct {
    uint32_t t;   /* epoch da amostra; 0 = relogio ainda nao sincronizado */
    uint8_t h5;   /* 0-100 */
    uint8_t d7;   /* 0-100 */
} history_sample_t;

#define STORAGE_HEAT_DAYS 31

typedef struct {
    uint32_t day;        /* dias locais desde a epoch (0 = slot vazio) */
    float burn[24];      /* consumo acumulado por hora do dia */
} history_day_heat_t;

/* samples/max_samples: buffer do chamador (tipicamente HISTORY_MAX_SAMPLES
 * amostras); heat: buffer de STORAGE_HEAT_DAYS dias. Retorna false (sem
 * alterar out_count) se o cartao/arquivo nao existir ainda. */
bool storage_history_load(history_sample_t *samples, int max_samples, int *out_count,
                           history_day_heat_t heat[STORAGE_HEAT_DAYS], float *out_last_h5);
bool storage_history_save(const history_sample_t *samples, int count,
                           const history_day_heat_t heat[STORAGE_HEAT_DAYS], float last_h5);

#ifdef __cplusplus
}
#endif
