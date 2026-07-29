#include "app_state.h"

#include <string.h>
#include <time.h>

#include "bsp_display.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "time_sync.h"
#include "wifi_manager.h"

static const char *TAG = "APP_STATE";

/* ---- Definicoes dos globais (declarados extern em app_state.h) ---- */
uint8_t g_lang = 0;

app_state_id_t g_state = ST_BOOT;
app_state_id_t g_pending = ST_BOOT;
bool g_dirty = false;

UsageData g_usage = {0};
ModelStatus g_status = {.ok = false, .haikuUp = true, .sonnetUp = true, .opusUp = true, .fableUp = true};

ModelInfo g_models[NMODELS] = {
    {"Haiku", "claude-haiku-4-5-20251001", {0, 0}, 0},
    {"Sonnet", "claude-sonnet-5", {0, 0}, 0},
    {"Opus", "claude-opus-4-8", {0, 0}, 0},
    {"Fable", "claude-fable-5", {0, 0}, 0},
};
int g_probeIdx = 0;

TokenStats g_tok = {0, 0, 0, 0, 0};

EncryptedBlob g_blob;
bool g_hasToken = false;
bool g_onboarding = false;
char g_token[300] = {0};
char g_pendingToken[300] = {0};
char g_pinEntry[PIN_LEN + 1] = {0};
char g_pinFirst[PIN_LEN + 1] = {0};
bool g_pinConfirming = false;
int g_pinAttempts = 0;
uint32_t g_lockoutUntil = 0;
bool g_timeInit = false;
volatile bool g_tokenGot = false;
bool g_pinRequired = true;
bool g_pinSetupFromSettings = false;

bool g_wantRefresh = false;
bool g_refreshing = false;
bool g_lastFetchOk = true;
uint32_t g_lastOkMs = 0;
lv_obj_t *g_hdrStatus = NULL;
lv_obj_t *g_hdrClock = NULL;

int g_briIdx = 1;
int g_pollSec = DEFAULT_POLL_SEC;
int g_tzOffset = -3;
int g_slideSec = 0;
int g_heatMode = 3;
char g_ntpServer[NTP_SERVER_MAX_LEN] = NTP_SERVER_1;
uint32_t g_lastPollMs = 0;
uint32_t g_lastTouchMs = 0;
uint32_t g_lastSlideMs = 0;

Sample g_hist[HIST_MAX];
int g_histN = 0;
int g_histHead = 0;
float g_hourBurn[24] = {0};
float g_lastH5 = -1.0f;

history_day_heat_t g_days[NDAYS];
int g_dayN = 0;

Mascot g_masc[NMODELS];
int g_mascN = 0;
lv_point_precise_t g_mXPts[NMODELS][4][2];

DashUI g_ui;
int g_curTile = 0;

lv_point_precise_t g_trPts[HIST_MAX];
lv_point_precise_t g_trProjPts[2];

MomentUI g_mo;
uint32_t g_momentUntil = 0;
int g_pendWin = -1, g_pendThr = 0;
uint8_t g_thrFired[2] = {0, 0};
float g_thrPrev[2] = {-1, -1};
bool g_thrBase = false;
lv_point_precise_t g_moXPts[4][2];

static const uint8_t BRI_LEVELS[3] = {60, 160, 255};

uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

void request_state(app_state_id_t s)
{
    g_pending = s;
    g_dirty = true;
}

void load_persisted(void)
{
    g_hasToken = storage_load_token_blob(&g_blob);
    g_pinAttempts = storage_load_pin_attempts();

    device_settings_t settings;
    storage_load_settings(&settings);
    g_pollSec = settings.poll_sec;
    if (g_pollSec < MIN_POLL_SEC || g_pollSec > MAX_POLL_SEC) {
        g_pollSec = DEFAULT_POLL_SEC;
    }
    g_slideSec = settings.slideshow_sec;
    if (g_slideSec != 0 && g_slideSec != 5 && g_slideSec != 10 && g_slideSec != 15 && g_slideSec != 30) {
        g_slideSec = 0;
    }
    g_tzOffset = settings.tz_offset_hours;
    if (g_tzOffset < -12 || g_tzOffset > 14) {
        g_tzOffset = -3;
    }
    g_briIdx = settings.brightness_idx;
    if (g_briIdx < 0 || g_briIdx > 2) {
        g_briIdx = 1;
    }
    g_lang = settings.language ? 1 : 0;
    g_heatMode = settings.heat_mode;
    if (g_heatMode < 0 || g_heatMode > 3) {
        g_heatMode = 3;
    }
    g_pinRequired = settings.pin_required != 0;

    storage_load_ntp_server(g_ntpServer, sizeof(g_ntpServer));
}

static void persist_settings(void)
{
    device_settings_t settings = {
        .poll_sec = (uint16_t)g_pollSec,
        .slideshow_sec = (uint8_t)g_slideSec,
        .tz_offset_hours = (int8_t)g_tzOffset,
        .brightness_idx = (uint8_t)g_briIdx,
        .language = g_lang,
        .heat_mode = (uint8_t)g_heatMode,
        .pin_required = g_pinRequired ? 1 : 0,
    };
    storage_save_settings(&settings);
}

bool auto_unlock_with_fixed_pin(void)
{
    return decryptToken(&g_blob, FIXED_PIN, g_token, sizeof(g_token));
}

void save_settings(void)
{
    persist_settings();
}

void save_blob(void)
{
    storage_save_token_blob(&g_blob);
}

void save_attempts(void)
{
    storage_save_pin_attempts(g_pinAttempts);
}

void apply_brightness(void)
{
    bsp_display_set_brightness(BRI_LEVELS[g_briIdx]);
    persist_settings();
}

void factory_reset(void)
{
    storage_factory_reset();
    wifi_manager_forget_all();
    g_hasToken = false;
    g_token[0] = '\0';
    g_pendingToken[0] = '\0';
    g_pinAttempts = 0;
    g_onboarding = true;
    ESP_LOGW(TAG, "[RESET] tudo apagado");
}

void apply_tz(void)
{
    persist_settings();
    /* O sistema fica sempre em UTC (SNTP); o deslocamento de fuso e somado
     * manualmente onde a hora local e formatada (fmt_clock/fmt_hm,
     * accumulate_heat/day_key) em vez de alterar TZ do processo. */
}

void ensure_time(void)
{
    if (g_timeInit || !wifi_manager_is_connected()) {
        return;
    }
    time_sync_start();
    g_timeInit = true;
    ESP_LOGI(TAG, "SNTP: sync iniciado");
}

void hist_push(float h5, float d7)
{
    time_t now = time_sync_now_utc();
    g_hist[g_histHead].t = (now > 1000000000L) ? (uint32_t)now : 0;
    g_hist[g_histHead].h5 = (uint8_t)(h5 + 0.5f);
    g_hist[g_histHead].d7 = (uint8_t)(d7 + 0.5f);
    g_histHead = (g_histHead + 1) % HIST_MAX;
    if (g_histN < HIST_MAX) {
        g_histN++;
    }
}

int hist_idx(int i)
{
    return (g_histHead - g_histN + i + HIST_MAX * 2) % HIST_MAX;
}

uint32_t day_key(void)
{
    time_t now = time_sync_now_utc();
    if (now < 1000000000L) {
        return 0;
    }
    return (uint32_t)(((long)now + (long)g_tzOffset * 3600) / 86400);
}

static int day_slot(uint32_t dk)
{
    for (int i = 0; i < g_dayN; i++) {
        if (g_days[i].day == dk) {
            return i;
        }
    }
    if (g_dayN == NDAYS) {
        memmove(&g_days[0], &g_days[1], sizeof(history_day_heat_t) * (NDAYS - 1));
        g_dayN--;
    }
    int i = g_dayN++;
    g_days[i].day = dk;
    memset(g_days[i].burn, 0, sizeof(g_days[i].burn));
    return i;
}

void accumulate_heat(float h5)
{
    time_t now = time_sync_now_utc();
    if (g_lastH5 >= 0 && now > 1000000000L) {
        float d = h5 - g_lastH5;
        if (d > 0 && d < 100) {
            time_t local_t = (time_t)((long)now + (long)g_tzOffset * 3600);
            struct tm tmv;
            gmtime_r(&local_t, &tmv);
            g_hourBurn[tmv.tm_hour] += d;
            uint32_t dk = day_key();
            if (dk) {
                g_days[day_slot(dk)].burn[tmv.tm_hour] += d;
            }
        }
    }
    g_lastH5 = h5;
}

void heat_mode_data(int mode, float out[24])
{
    memset(out, 0, sizeof(float) * 24);
    if (mode == 3) {
        memcpy(out, g_hourBurn, sizeof(float) * 24);
        return;
    }
    uint32_t today = day_key();
    if (!today) {
        return;
    }
    uint32_t minDay = (mode == 0) ? today : (mode == 1) ? today - 6 : today - 29;
    for (int i = 0; i < g_dayN; i++) {
        if (g_days[i].day < minDay || g_days[i].day > today) {
            continue;
        }
        for (int h = 0; h < 24; h++) {
            out[h] += g_days[i].burn[h];
        }
    }
}

void save_history(void)
{
    storage_history_save(g_hist, g_histN, g_days, g_lastH5);
}

void load_history(void)
{
    int count = 0;
    float last_h5 = -1.0f;
    if (storage_history_load(g_hist, HIST_MAX, &count, g_days, &last_h5)) {
        g_histN = count;
        g_histHead = count % HIST_MAX;
        g_lastH5 = last_h5;
        g_dayN = 0;
        for (int i = 0; i < NDAYS; i++) {
            if (g_days[i].day != 0) {
                g_dayN = i + 1;
            }
        }
        for (int h = 0; h < 24; h++) {
            g_hourBurn[h] = 0;
            for (int i = 0; i < g_dayN; i++) {
                g_hourBurn[h] += g_days[i].burn[h];
            }
        }
        ESP_LOGI(TAG, "historico carregado: %d amostras, %d dias de heatmap", g_histN, g_dayN);
    } else {
        ESP_LOGI(TAG, "sem HISTORY.DAT anterior; iniciando historico vazio");
    }
}

void probe_next_model(void)
{
    int mi = g_probeIdx % NMODELS;
    g_probeIdx++;
    probeModel(g_token, g_models[mi].id, &g_models[mi].pr);
    g_models[mi].atMs = now_ms();
}

void nav_cb(lv_event_t *e)
{
    app_state_id_t s = (app_state_id_t)(intptr_t)lv_event_get_user_data(e);
    request_state(s);
}

void render_state(void)
{
    g_state = g_pending;
    moment_close();
    lv_obj_clean(lv_layer_top());

    memset(&g_ui, 0, sizeof(g_ui));
    g_mascN = 0;
    g_hdrStatus = NULL;
    g_hdrClock = NULL;

    lv_obj_t *scr = lv_screen_active();
    lv_obj_clean(scr);
    lv_obj_set_style_bg_color(scr, lv_color_hex(C_BG), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    switch (g_state) {
    case ST_PIN:
    case ST_SETUP_PIN:
        ui_pin();
        break;
    case ST_WIFI:
        ui_wifi();
        break;
    case ST_TOKEN:
        ui_token();
        break;
    case ST_LOADING:
        ui_loading(wifi_manager_is_connected() ? wifi_manager_get_ssid()
                                                : TRS("conectando WiFi", "connecting WiFi"));
        break;
    case ST_MAIN:
        ui_main();
        break;
    case ST_SETTINGS:
        ui_settings();
        break;
    case ST_ABOUT:
        ui_about();
        break;
    case ST_NTP_SERVER:
        ui_ntp_server();
        break;
    case ST_ERROR:
        ui_message(TRS("Falha", "Failed"), g_usage.error[0] ? g_usage.error : TRS("sem dados", "no data"), C_BAD);
        break;
    default:
        break;
    }
}
