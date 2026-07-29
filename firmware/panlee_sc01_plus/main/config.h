#pragma once

/* ============================================================
 * Claude Usage Stick — Panlee SC01 Plus (ESP32-S3, ST7796UI i80)
 * Pinos: ver board.h e REFERENCIA-HARDWARE-LVGL.md
 * ============================================================ */

#define FW_VERSION               "1.0.0-sc01plus"

/* Polling */
#define DEFAULT_POLL_SEC         120
#define MIN_POLL_SEC             30
#define MAX_POLL_SEC             300
#define STATUS_POLL_SEC          300     /* status.claude.com a cada 5 min */

/* Seguranca (PIN + AES-256-GCM) */
#define PIN_LEN                  4
#define MAX_PIN_ATTEMPTS         10
#define LOCKOUT_BASE_SEC         60      /* dobra a cada falha */
#define KDF_ROUNDS               10000

/* PIN fixo usado quando "Solicitar PIN no boot" esta desligado nos Ajustes.
 * NAO e secreto — desligar essa opcao reduz a protecao do token a apenas
 * ofuscacao contra um dump da flash, nao contra o uso direto do aparelho
 * ja ligado (ver README, secao Seguranca). */
#define FIXED_PIN                "0000"

/* Rede / API Claude */
#define WIFI_CONNECT_TIMEOUT_MS  8000
#define API_TIMEOUT_MS           15000
#define MESSAGES_ENDPOINT        "https://api.anthropic.com/v1/messages"
#define ANTHROPIC_VERSION        "2023-06-01"
#define PROBE_MODEL              "claude-haiku-4-5-20251001"
/* status.anthropic.com redireciona para ca — consultar o host canonico direto */
#define STATUS_ENDPOINT          "https://status.claude.com/api/v2/incidents/unresolved.json"
#define ANTHROPIC_BETA_HEADER    "oauth-2025-04-20"
#define CLAUDE_CODE_USER_AGENT   "claude-code/2.1.5"

/* O token colado no onboarding PRECISA ser gerado com `claude setup-token`
 * (Claude Code CLI) — nao e uma chave de API comum (essas comecam com
 * "sk-ant-api03-" e sao rejeitadas pela API para esta chamada, que depende
 * do fluxo OAuth do Claude Code). Validado localmente pelo prefixo antes de
 * gastar uma chamada de rede — ver onboarding_on_token_submitted() em
 * ui_onboarding.c. */
#define OAUTH_TOKEN_PREFIX       "sk-ant-oat01-"

/* NTP (necessario para os contadores de reset). NTP_SERVER_1 e so o DEFAULT
 * de fabrica — o valor realmente usado fica em g_ntpServer (app_state.c),
 * editavel em Ajustes -> Servidor NTP e persistido em NVS. pool.ntp.br =
 * pool nacional (NIC.br), menor latencia no Brasil. time.cloudflare.com e
 * fixo como fallback secundario (anycast, quase sempre tem PoP proximo). */
#define NTP_SERVER_1             "pool.ntp.br"
#define NTP_SERVER_2             "time.cloudflare.com"
#define NTP_SERVER_MAX_LEN       64

/* NVS */
#define NVS_NAMESPACE            "claude"
#define NVS_NAMESPACE_WIFI       "wifi"

/* Onboarding web + mDNS */
#define ONBOARDING_HOSTNAME      "claude-stick"
#define ONBOARDING_HTTP_PORT     80

/* Historico/heatmap no microSD (nome 8.3). HISTORY_MAX_SAMPLES cobre so a
 * janela de tendencia de 5h (ring buffer curto); a retencao de 31 dias vem
 * do heatmap por dia (STORAGE_HEAT_DAYS em storage.h), nao das amostras
 * brutas. Com poll de 120s, 160 amostras cobrem ~5h20min. */
#define HISTORY_FILE_PATH        "/sdcard/HISTORY.DAT"
#define HISTORY_MAX_SAMPLES      160
