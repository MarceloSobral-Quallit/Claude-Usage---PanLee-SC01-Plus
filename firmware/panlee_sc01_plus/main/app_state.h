#pragma once

/*
 * Estado compartilhado da aplicacao (porte do bloco de globais de
 * claude_stick.ino). Centralizado aqui porque a UI original era um unico
 * arquivo com "static" de escopo de arquivo; ao dividir em
 * ui_common.c/ui_onboarding.c/ui_dashboard.c/ui_settings.c/ui_moments.c
 * essas variaveis precisam de linkage externa para serem compartilhadas.
 *
 * Mantem os mesmos nomes/semantica do original sempre que possivel para
 * facilitar comparacao com claude-usage-stick-SVGL-main/firmware/claude_stick/claude_stick.ino.
 */

#include <stdbool.h>
#include <stdint.h>

#include "api.h"
#include "config.h"
#include "crypto.h"
#include "lvgl.h"
#include "status.h"
#include "storage.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Paleta (escuro, minimalista; acento coral do Claude) ---- */
#define C_BG       0x0F0F12
#define C_SURFACE  0x1A1A20
#define C_SURFACE2 0x24242C
#define C_TRACK    0x26262E
#define C_GRID     0x232329
#define C_BORDER   0x30303A
#define C_TEXT     0xF2F0EC
#define C_MUTED    0x8C8C98
#define C_FAINT    0x5C5C68
#define C_ACCENT   0xD97757
#define C_OK       0x4ADE80
#define C_WARN     0xFBBF24
#define C_BAD      0xF87171

/* ---- Idioma (0 = portugues, 1 = english; Ajustes -> NVS "lang") ---- */
extern uint8_t g_lang;
#define TRS(pt, en) (g_lang ? (en) : (pt))

/* ---- Estado da aplicacao ---- */
typedef enum {
    ST_BOOT, ST_PIN, ST_SETUP_PIN, ST_WIFI, ST_TOKEN,
    ST_LOADING, ST_MAIN, ST_SETTINGS, ST_ABOUT, ST_ERROR
} app_state_id_t;

extern app_state_id_t g_state;
extern app_state_id_t g_pending;
extern bool g_dirty;
void request_state(app_state_id_t s);

/* ---- Dados ---- */
extern UsageData g_usage;
extern ModelStatus g_status;

#define NMODELS 4
typedef struct {
    const char *name;
    const char *id;
    ProbeResult pr;
    uint32_t atMs;
} ModelInfo;
extern ModelInfo g_models[NMODELS];
extern int g_probeIdx;

typedef struct {
    long long tin, tout, cache;
    int sessions;
    uint32_t atMs;
} TokenStats;
extern TokenStats g_tok;
#define TOK_FRESH_MS (15UL * 60UL * 1000UL)

/* ---- Token / seguranca ---- */
extern EncryptedBlob g_blob;
extern bool g_hasToken;
extern bool g_onboarding;
extern char g_token[300];
extern char g_pendingToken[300];
extern char g_pinEntry[PIN_LEN + 1];
extern char g_pinFirst[PIN_LEN + 1];
extern bool g_pinConfirming;
extern int g_pinAttempts;
extern uint32_t g_lockoutUntil;
extern bool g_timeInit;

/* "Solicitar PIN no boot" (Ajustes). Quando false, o boot e a troca de
 * token usam FIXED_PIN automaticamente em vez de pedir na tela — ver
 * auto_unlock_with_fixed_pin() e pin_submit() (ui_onboarding.c). */
extern bool g_pinRequired;
/* true quando ST_SETUP_PIN foi aberto a partir de Ajustes (religar a
 * exigencia de PIN) em vez do onboarding normal — muda o que pin_submit()
 * faz ao terminar (re-cifra g_token e volta pros Ajustes, em vez de seguir
 * o fluxo de onboarding) e habilita o botao "Cancelar" na tela. */
extern bool g_pinSetupFromSettings;
/* Tenta decifrar o blob salvo com FIXED_PIN; usado no boot quando
 * g_pinRequired e false. Retorna false (sem alterar g_token) se falhar. */
bool auto_unlock_with_fixed_pin(void);
extern volatile bool g_tokenGot; /* onboarding_server -> loop: token validado, ir para ST_SETUP_PIN */

/* ---- Refresh em background ---- */
extern bool g_wantRefresh;
extern bool g_refreshing;
extern bool g_lastFetchOk;
extern uint32_t g_lastOkMs;
extern lv_obj_t *g_hdrStatus;

/* ---- Configuracoes (NVS via storage.c) ---- */
extern int g_briIdx;
extern int g_pollSec;
extern int g_tzOffset;
extern int g_slideSec;
extern int g_heatMode;
extern uint32_t g_lastPollMs;
extern uint32_t g_lastTouchMs;
extern uint32_t g_lastSlideMs;

/* ---- Historico (ring buffer; persistido no microSD via storage.c) ---- */
#define HIST_MAX HISTORY_MAX_SAMPLES
typedef history_sample_t Sample;
extern Sample g_hist[HIST_MAX];
extern int g_histN;
extern int g_histHead;
extern float g_hourBurn[24];
extern float g_lastH5;

/* ---- Heatmap por dia ---- */
#define NDAYS STORAGE_HEAT_DAYS
extern history_day_heat_t g_days[NDAYS];
extern int g_dayN;

/* ---- Mascotes Clawd (pagina de modelos) ---- */
typedef struct {
    lv_obj_t *cont, *img, *lid[2], *drop;
    int baseY, mood;
} Mascot;
extern Mascot g_masc[NMODELS];
extern int g_mascN;
extern lv_point_precise_t g_mXPts[NMODELS][4][2];

/* ---- Ponteiros de UI do dashboard (zerados a cada render_state) ---- */
#define NTILES 4
#define NSEG 18
typedef struct {
    lv_obj_t *tv, *tile[NTILES], *dots[NTILES];
    lv_obj_t *refBar;
    lv_obj_t *agChip, *agPct5, *agCd5, *agAt5;
    lv_obj_t *agPct7, *agCd7, *agAt7, *agTok;
    lv_obj_t *seg5[NSEG], *seg7[NSEG];
    lv_obj_t *mChip[NMODELS], *incident;
    lv_obj_t *trHist, *trProj, *trDot, *trCap, *trT0, *trT1;
    lv_obj_t *heat[24], *heatBtn[4];
} DashUI;
extern DashUI g_ui;
extern int g_curTile;

extern lv_point_precise_t g_trPts[HIST_MAX];
extern lv_point_precise_t g_trProjPts[2];

/* ---- Momentos (overlay de limiar) ---- */
typedef struct {
    lv_obj_t *scrim, *box, *img, *pct, *seg[NSEG];
    lv_obj_t *lid[2], *drop[2], *ring, *xline[4];
    int win, thr, fromPct;
    int boxY;
    uint32_t t0;
} MomentUI;
extern MomentUI g_mo;
extern uint32_t g_momentUntil;
extern int g_pendWin, g_pendThr;
extern uint8_t g_thrFired[2];
extern float g_thrPrev[2];
extern bool g_thrBase;
extern lv_point_precise_t g_moXPts[4][2];

/* ---- Helpers de tempo (ms desde o boot) ---- */
uint32_t now_ms(void);

/* ---- Ciclo de vida / persistencia (app_state.c) ---- */
void load_persisted(void);
void save_blob(void);
void save_attempts(void);
/* Persiste g_pollSec/g_slideSec/g_tzOffset/g_briIdx/g_lang/g_heatMode na
 * NVS. Chame apos alterar qualquer um desses globais fora de
 * apply_brightness()/apply_tz() (que ja persistem sozinhos). */
void save_settings(void);
void apply_brightness(void);
void factory_reset(void);
void apply_tz(void);
void ensure_time(void);
void hist_push(float h5, float d7);
int hist_idx(int i);
uint32_t day_key(void);
void accumulate_heat(float h5);
void heat_mode_data(int mode, float out[24]);
void save_history(void);
void load_history(void);
void render_state(void);
void nav_cb(lv_event_t *e);

/* Disparados apos um fetch bem-sucedido (rodam na task de refresh, ver
 * app_main.c); atualizam historico/heatmap/mascotes e persistem no SD. */
void probe_next_model(void);
void refresh_begin(bool is_initial);

/* ---- ui_common.c ---- */
lv_obj_t *mklabel(lv_obj_t *p, const char *txt, const lv_font_t *font, uint32_t color);
void no_box(lv_obj_t *o);
lv_obj_t *mkbtn(lv_obj_t *p, const char *txt, const lv_font_t *font, uint32_t bg, uint32_t fg);
uint32_t pct_color(float p);
lv_color_t grad_color(float p);
void set_meter(lv_obj_t **seg, float pct);
void fmt_eta(uint32_t epoch, char *out, int sz);
void fmt_clock(uint32_t epoch, char *out, int sz);
void fmt_hm(uint32_t epoch, char *out, int sz);
void fmt_tok(long long v, char *out, int sz);
lv_obj_t *build_claude_mark(lv_obj_t *parent);
lv_obj_t *tlabel(lv_obj_t *p, const lv_font_t *f, uint32_t c, int x, int y);
lv_obj_t *tstatic(lv_obj_t *p, const char *txt, const lv_font_t *f, uint32_t c, int x, int y);
void tile_setup(lv_obj_t *t);
lv_obj_t *card(lv_obj_t *p, int x, int y, int w, int h);
lv_obj_t *mkchip(lv_obj_t *p, int x, int y);
void set_chip(lv_obj_t *o, const char *txt, uint32_t col);
lv_obj_t *rrect(lv_obj_t *p, int x, int y, int w, int h, int r, uint32_t col);
uint32_t status_color(const char *s);
const char *overall_label(const char *s);
void ui_message(const char *title, const char *sub, uint32_t color);

/* ---- ui_onboarding.c ---- */
void ui_pin(void);
void ui_wifi(void);
void ui_token(void);
void ui_loading(const char *sub);
/* Chamada a cada iteracao do loop principal enquanto g_state==ST_TOKEN;
 * aplica na tela (thread-safe, so a partir da task principal) o resultado
 * da validacao feita em on_token_submitted() (essa sim roda na task do
 * esp_http_server). */
void ui_token_tick(void);

/* Callbacks registrados em onboarding_server_start() (app_main.c). Rodam na
 * task do esp_http_server; so mexem em estado puro, nunca em objetos LVGL
 * (ver ui_token_tick()). */
bool onboarding_is_token_configured(void);
bool onboarding_on_token_submitted(const char *token);
/* Motivo da ultima rejeicao (formato curto do prefixo/duplicata da validacao,
 * ver ui_onboarding.c); usado pelo servidor web para mostrar o erro real na
 * pagina de resposta em vez de uma mensagem generica. */
const char *onboarding_get_token_error(void);
uint32_t onboarding_get_h5_reset_epoch(void);
void onboarding_on_tokens_pushed(long long tin, long long tout, long long cache, int sessions);

/* ---- ui_dashboard.c ---- */
void ui_main(void);
void refresh_ui_values(void);
void set_hdr_status(void);
void dash_tick(void);
void trend_redraw(void);
void heat_redraw(void);
void update_tok_row(void);
void dashboard_animate(uint32_t now);
int model_mood(int i);

/* ---- ui_settings.c ---- */
void ui_settings(void);
void ui_about(void);

/* ---- ui_moments.c ---- */
void check_thresholds(void);
void moment_close(void);
void show_moment(int win, int thr);
void moment_tick(void);

#ifdef __cplusplus
}
#endif
