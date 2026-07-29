#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    /* true se ja existe um token configurado (GET / decide entre o
     * formulario de onboarding e a pagina de status). */
    bool (*is_token_configured)(void);

    /* Recebe o token colado no formulario web. Deve validar (chamada real
     * a API) e persistir; retorna true se aceito. */
    bool (*on_token_submitted)(const char *token);

    /* Motivo textual da ultima rejeicao de on_token_submitted(), mostrado na
     * pagina de resposta ao usuario (ex.: prefixo errado, token recusado
     * pela API). Pode ser NULL. */
    const char *(*get_token_error)(void);

    /* GET /window — usado pelo tools/token_bridge.py para saber o inicio
     * da janela de 5h atual. */
    uint32_t (*get_h5_reset_epoch)(void);

    /* POST /tokens — recebe os totais de tokens da janela atual, calculados
     * localmente pelo tools/token_bridge.py a partir dos transcripts. */
    void (*on_tokens_pushed)(long long tin, long long tout, long long cache, int sessions);
} onboarding_server_callbacks_t;

void onboarding_server_start(const onboarding_server_callbacks_t *callbacks);
void onboarding_server_stop(void);

#ifdef __cplusplus
}
#endif
