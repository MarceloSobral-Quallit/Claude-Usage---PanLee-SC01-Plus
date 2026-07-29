#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool ok;
    float h5;                    /* 0-100 (%) */
    float d7;                    /* 0-100 (%) */
    uint32_t h5ResetEpoch;
    uint32_t d7ResetEpoch;
    uint32_t unifiedResetEpoch;
    float fallbackPct;
    char statusOverall[24];      /* allowed | allowed_warning | rejected */
    char status5h[24];
    char status7d[24];
    char repClaim[24];           /* five_hour | seven_day */
    char overageStatus[24];
    char overageReason[64];
    char error[32];
} UsageData;

typedef struct {
    int code;
    uint16_t ms;
} ProbeResult;

/* POST minimo (max_tokens=1) em MESSAGES_ENDPOINT; le o uso dos headers
 * anthropic-ratelimit-unified-*. Nao usa o corpo da resposta. */
bool fetchUsage(const char *token, UsageData *out);

/* Mesma chamada, mas so mede codigo HTTP + latencia, para o probe de saude
 * por modelo (Haiku/Sonnet/Opus/Fable). */
bool probeModel(const char *token, const char *modelId, ProbeResult *out);

#ifdef __cplusplus
}
#endif
