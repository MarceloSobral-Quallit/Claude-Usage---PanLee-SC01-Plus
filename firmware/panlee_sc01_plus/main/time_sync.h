#pragma once

#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Inicia o SNTP (NTP_SERVER_1/2 de config.h) em background; nao bloqueia.
 * Idempotente — chamadas repetidas nao reiniciam o cliente SNTP. */
void time_sync_start(void);

/* true assim que o relogio do sistema parecer valido (ano >= 2024). Os
 * contadores de reset das janelas 5h/7d dependem disso. */
bool time_sync_is_synced(void);

/* Epoch UTC atual (0 se o relogio ainda nao sincronizou). A conversao para
 * hora local (GMT+-N) e feita na UI somando tz_offset_hours*3600 — o
 * sistema em si permanece em UTC. */
time_t time_sync_now_utc(void);

#ifdef __cplusplus
}
#endif
