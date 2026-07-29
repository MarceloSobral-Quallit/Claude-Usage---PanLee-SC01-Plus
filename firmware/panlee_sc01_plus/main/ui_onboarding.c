#include "app_state.h"
#include "logo_assets.h"
#include "onboarding_server.h"
#include "wifi_manager.h"

#include <stdio.h>
#include <string.h>

#include "esp_log.h"

static const char *TAG = "UI_ONBOARD";

/* ============================================================
 * Tela: PIN (keypad touch)
 * ============================================================ */
static lv_obj_t *g_pinDots = NULL, *g_pinMsg = NULL;

static const char *pin_map[] = {
    "1", "2", "3", "\n",
    "4", "5", "6", "\n",
    "7", "8", "9", "\n",
    LV_SYMBOL_LEFT, "0", LV_SYMBOL_OK, "",
};

static void pin_update_dots(void)
{
    if (!g_pinDots) {
        return;
    }
    char dots[24] = {0};
    int len = (int)strlen(g_pinEntry);
    for (int i = 0; i < PIN_LEN; i++) {
        strcat(dots, i < len ? "*" : "_");
        if (i < PIN_LEN - 1) {
            strcat(dots, " ");
        }
    }
    lv_label_set_text(g_pinDots, dots);
}

static void pin_submit(void)
{
    if (g_state == ST_SETUP_PIN) {
        if (!g_pinConfirming) {
            snprintf(g_pinFirst, sizeof(g_pinFirst), "%s", g_pinEntry);
            g_pinConfirming = true;
            g_pinEntry[0] = 0;
            pin_update_dots();
            if (g_pinMsg) {
                lv_label_set_text(g_pinMsg, TRS("Confirme o PIN", "Confirm the PIN"));
            }
            return;
        }
        if (strcmp(g_pinFirst, g_pinEntry) != 0) {
            g_pinConfirming = false;
            g_pinFirst[0] = 0;
            g_pinEntry[0] = 0;
            pin_update_dots();
            if (g_pinMsg) {
                lv_label_set_text(g_pinMsg, TRS("Nao bateu. Defina de novo.", "Didn't match. Set it again."));
            }
            return;
        }
        /* Onboarding normal cifra o token recem-validado (g_pendingToken);
         * religar o PIN pelos Ajustes re-cifra o token ja decifrado em uso
         * (g_token) — nenhum dos dois mexe no outro. */
        const char *token_to_encrypt = g_pinSetupFromSettings ? g_token : g_pendingToken;
        if (!encryptToken(token_to_encrypt, g_pinEntry, &g_blob)) {
            if (g_pinMsg) {
                lv_label_set_text(g_pinMsg, TRS("Falha ao cifrar. Tente de novo.", "Encryption failed. Try again."));
            }
            g_pinConfirming = false;
            g_pinFirst[0] = 0;
            g_pinEntry[0] = 0;
            pin_update_dots();
            return;
        }
        save_blob();
        g_pinConfirming = false;
        g_pinFirst[0] = 0;
        g_pinEntry[0] = 0;

        if (g_pinSetupFromSettings) {
            g_pinSetupFromSettings = false;
            g_pinRequired = true;
            save_settings();
            ESP_LOGI(TAG, "PIN no boot religado pelos Ajustes");
            request_state(ST_SETTINGS);
            return;
        }

        snprintf(g_token, sizeof(g_token), "%s", g_pendingToken);
        memset(g_pendingToken, 0, sizeof(g_pendingToken));
        g_hasToken = true;
        g_onboarding = false;
        g_pinAttempts = 0;
        save_attempts();
        ESP_LOGI(TAG, "token cifrado e salvo");
        request_state(wifi_manager_is_connected() ? ST_LOADING : ST_WIFI);
        return;
    }

    /* ST_PIN: tenta decifrar */
    if (decryptToken(&g_blob, g_pinEntry, g_token, sizeof(g_token))) {
        g_pinAttempts = 0;
        save_attempts();
        g_pinEntry[0] = 0;
        ESP_LOGI(TAG, "PIN ok, token %d chars", (int)strlen(g_token));
        if (!wifi_manager_is_connected()) {
            wifi_manager_autoconnect(WIFI_CONNECT_TIMEOUT_MS);
        }
        request_state(wifi_manager_is_connected() ? ST_LOADING : ST_WIFI);
    } else {
        g_pinAttempts++;
        save_attempts();
        g_pinEntry[0] = 0;
        pin_update_dots();
        if (g_pinAttempts >= MAX_PIN_ATTEMPTS) {
            ESP_LOGW(TAG, "limite de tentativas de PIN estourado -> wipe");
            factory_reset();
            request_state(ST_WIFI);
            return;
        }
        int wait = LOCKOUT_BASE_SEC * (1 << (g_pinAttempts - 1));
        if (wait > 3600) {
            wait = 3600;
        }
        g_lockoutUntil = now_ms() + (uint32_t)wait * 1000;
        if (g_pinMsg) {
            char m[64];
            snprintf(m, sizeof(m), TRS("PIN errado (%d/%d). Aguarde %ds", "Wrong PIN (%d/%d). Wait %ds"),
                     g_pinAttempts, MAX_PIN_ATTEMPTS, wait);
            lv_label_set_text(g_pinMsg, m);
        }
    }
}

static void pin_kb_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) {
        return;
    }
    if (now_ms() < g_lockoutUntil) {
        return;
    }
    lv_obj_t *bm = (lv_obj_t *)lv_event_get_target(e);
    uint32_t id = lv_buttonmatrix_get_selected_button(bm);
    const char *txt = lv_buttonmatrix_get_button_text(bm, id);
    if (!txt) {
        return;
    }
    int len = (int)strlen(g_pinEntry);
    if (strcmp(txt, LV_SYMBOL_LEFT) == 0) {
        if (len > 0) {
            g_pinEntry[len - 1] = 0;
        }
        pin_update_dots();
    } else if (strcmp(txt, LV_SYMBOL_OK) == 0) {
        if (len == PIN_LEN) {
            pin_submit();
        }
    } else if (len < PIN_LEN) {
        g_pinEntry[len] = txt[0];
        g_pinEntry[len + 1] = 0;
        pin_update_dots();
        if (len + 1 == PIN_LEN) {
            pin_submit();
        }
    }
}

static void pin_setup_cancel_cb(lv_event_t *e)
{
    (void)e;
    g_pinSetupFromSettings = false;
    g_pinConfirming = false;
    g_pinFirst[0] = 0;
    g_pinEntry[0] = 0;
    request_state(ST_SETTINGS);
}

void ui_pin(void)
{
    lv_obj_t *scr = lv_screen_active();
    const char *title = (g_state == ST_SETUP_PIN)
                             ? (g_pinConfirming ? TRS("Confirme o PIN", "Confirm the PIN") : TRS("Defina um PIN", "Set a PIN"))
                             : TRS("Digite o PIN", "Enter the PIN");
    lv_obj_t *t = mklabel(scr, title, &lv_font_montserrat_22, C_TEXT);
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 14);

    g_pinDots = mklabel(scr, "", &lv_font_montserrat_28, C_ACCENT);
    lv_obj_align(g_pinDots, LV_ALIGN_TOP_MID, 0, 48);
    pin_update_dots();

    const char *sub = (g_state == ST_SETUP_PIN) ? TRS("Voce vai digita-lo a cada boot.", "You'll type it on every boot.")
                                                 : TRS("Necessario para desbloquear o token.", "Needed to unlock the token.");
    g_pinMsg = mklabel(scr, sub, &lv_font_montserrat_14, C_MUTED);
    lv_obj_align(g_pinMsg, LV_ALIGN_TOP_MID, 0, 86);

    if (g_state == ST_SETUP_PIN && g_pinSetupFromSettings) {
        lv_obj_t *cancel = mkbtn(scr, TRS(LV_SYMBOL_CLOSE " Cancelar", LV_SYMBOL_CLOSE " Cancel"),
                                  &lv_font_montserrat_14, C_SURFACE2, C_MUTED);
        lv_obj_set_size(cancel, 120, 34);
        lv_obj_align(cancel, LV_ALIGN_TOP_RIGHT, -10, 10);
        lv_obj_add_event_cb(cancel, pin_setup_cancel_cb, LV_EVENT_CLICKED, NULL);
    }

    lv_obj_t *bm = lv_buttonmatrix_create(scr);
    lv_buttonmatrix_set_map(bm, pin_map);
    lv_obj_set_size(bm, 280, 180);
    lv_obj_align(bm, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_style_bg_color(bm, lv_color_hex(C_BG), 0);
    lv_obj_set_style_border_width(bm, 0, 0);
    lv_obj_set_style_text_font(bm, &lv_font_montserrat_24, 0);
    lv_obj_set_style_bg_color(bm, lv_color_hex(C_SURFACE2), LV_PART_ITEMS);
    lv_obj_set_style_text_color(bm, lv_color_hex(C_TEXT), LV_PART_ITEMS);
    lv_obj_add_event_cb(bm, pin_kb_cb, LV_EVENT_VALUE_CHANGED, NULL);

    if (now_ms() < g_lockoutUntil && g_pinMsg) {
        int rem = (int)((g_lockoutUntil - now_ms()) / 1000);
        char m[48];
        snprintf(m, sizeof(m), TRS("Aguarde %ds", "Wait %ds"), rem);
        lv_label_set_text(g_pinMsg, m);
    }
}

/* ============================================================
 * Tela: WiFi (scan + teclado)
 * ============================================================ */
static lv_obj_t *wifi_list = NULL, *wifi_ta = NULL, *wifi_kb = NULL, *wifi_status = NULL;
static char sel_ssid[33] = {0};

static void wifi_item_cb(lv_event_t *e);

static void wifi_populate(void)
{
    lv_obj_clean(wifi_list);
    lv_label_set_text(wifi_status, TRS("Escaneando redes...", "Scanning networks..."));
    lv_refr_now(NULL);
    wifi_scan_result_t nets[12];
    int n = wifi_manager_scan(nets, 12);
    for (int i = 0; i < n; i++) {
        lv_obj_t *b = lv_list_add_button(wifi_list, LV_SYMBOL_WIFI, nets[i].ssid);
        lv_obj_set_style_bg_color(b, lv_color_hex(C_SURFACE), 0);
        lv_obj_set_style_text_color(b, lv_color_hex(C_TEXT), 0);
        lv_obj_add_event_cb(b, wifi_item_cb, LV_EVENT_CLICKED, NULL);
    }
    lv_label_set_text(wifi_status, n > 0 ? TRS("Toque na sua rede", "Tap your network")
                                         : TRS("Nenhuma rede. Toque em Reescanear.", "No networks. Tap Rescan."));
}

static void wifi_item_cb(lv_event_t *e)
{
    lv_obj_t *btn = (lv_obj_t *)lv_event_get_target(e);
    const char *txt = lv_list_get_button_text(wifi_list, btn);
    if (!txt) {
        return;
    }
    snprintf(sel_ssid, sizeof(sel_ssid), "%s", txt);
    lv_label_set_text_fmt(wifi_status, TRS("Senha de \"%s\":", "Password for \"%s\":"), sel_ssid);
    lv_obj_add_flag(wifi_list, LV_OBJ_FLAG_HIDDEN);
    lv_textarea_set_text(wifi_ta, "");
    lv_obj_clear_flag(wifi_ta, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(wifi_kb, LV_OBJ_FLAG_HIDDEN);
    lv_keyboard_set_textarea(wifi_kb, wifi_ta);
}

static void wifi_kb_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_READY) {
        const char *pass = lv_textarea_get_text(wifi_ta);
        lv_label_set_text(wifi_status, TRS("Conectando...", "Connecting..."));
        lv_obj_add_flag(wifi_kb, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(wifi_ta, LV_OBJ_FLAG_HIDDEN);
        lv_refr_now(NULL);
        bool ok = wifi_manager_connect_to(sel_ssid, pass, 15000);
        if (ok) {
            request_state(g_onboarding ? ST_TOKEN : ST_LOADING);
        } else {
            lv_label_set_text(wifi_status, TRS("Falhou. Toque numa rede de novo.", "Failed. Tap a network again."));
            lv_obj_clear_flag(wifi_list, LV_OBJ_FLAG_HIDDEN);
        }
    } else if (code == LV_EVENT_CANCEL) {
        lv_obj_add_flag(wifi_kb, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(wifi_ta, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(wifi_list, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(wifi_status, TRS("Toque na sua rede", "Tap your network"));
    }
}

static void wifi_rescan_cb(lv_event_t *e)
{
    (void)e;
    wifi_populate();
}

void ui_wifi(void)
{
    lv_obj_t *scr = lv_screen_active();
    lv_obj_t *title = mklabel(scr, TRS("Configurar WiFi", "Configure WiFi"), &lv_font_montserrat_20, C_TEXT);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 14, 12);

    lv_obj_t *rb = mkbtn(scr, TRS("Reescanear", "Rescan"), &lv_font_montserrat_14, C_SURFACE2, C_ACCENT);
    lv_obj_align(rb, LV_ALIGN_TOP_RIGHT, -12, 8);
    lv_obj_add_event_cb(rb, wifi_rescan_cb, LV_EVENT_CLICKED, NULL);

    if (!g_onboarding && g_hasToken) {
        lv_obj_t *bk = mkbtn(scr, TRS(LV_SYMBOL_LEFT " Voltar", LV_SYMBOL_LEFT " Back"), &lv_font_montserrat_14,
                              C_SURFACE2, C_MUTED);
        lv_obj_align(bk, LV_ALIGN_TOP_RIGHT, -150, 8);
        lv_obj_add_event_cb(bk, nav_cb, LV_EVENT_CLICKED, (void *)(intptr_t)(g_usage.ok ? ST_MAIN : ST_SETTINGS));
    }

    wifi_status = mklabel(scr, "...", &lv_font_montserrat_14, C_MUTED);
    lv_obj_align(wifi_status, LV_ALIGN_TOP_LEFT, 14, 44);

    wifi_list = lv_list_create(scr);
    lv_obj_set_size(wifi_list, 452, 246);
    lv_obj_align(wifi_list, LV_ALIGN_TOP_MID, 0, 68);
    lv_obj_set_style_bg_color(wifi_list, lv_color_hex(C_BG), 0);
    lv_obj_set_style_border_color(wifi_list, lv_color_hex(C_BORDER), 0);

    wifi_ta = lv_textarea_create(scr);
    lv_textarea_set_one_line(wifi_ta, true);
    lv_textarea_set_password_mode(wifi_ta, true);
    lv_textarea_set_placeholder_text(wifi_ta, TRS("senha do WiFi", "WiFi password"));
    lv_obj_set_size(wifi_ta, 452, 44);
    lv_obj_align(wifi_ta, LV_ALIGN_TOP_MID, 0, 66);
    lv_obj_add_flag(wifi_ta, LV_OBJ_FLAG_HIDDEN);

    wifi_kb = lv_keyboard_create(scr);
    lv_obj_add_flag(wifi_kb, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(wifi_kb, wifi_kb_cb, LV_EVENT_ALL, NULL);

    wifi_populate();
}

/* ============================================================
 * Tela: Token (onboarding_server ja esta rodando desde o boot; aqui so
 * mostramos o IP e escutamos o resultado da validacao via ui_token_tick())
 * ============================================================ */
static lv_obj_t *g_tokMsg = NULL;

typedef enum { TOKVAL_IDLE, TOKVAL_VALIDATING, TOKVAL_OK, TOKVAL_REJECTED } tokval_state_t;
static volatile tokval_state_t s_tokval_state = TOKVAL_IDLE;
static char s_tokval_error[64] = {0};

bool onboarding_is_token_configured(void)
{
    return g_hasToken;
}

/* Roda na task do esp_http_server (POST /token). So mexe em estado puro
 * (sem chamadas LVGL); a tela e atualizada depois, em ui_token_tick(). */
bool onboarding_on_token_submitted(const char *token)
{
    size_t len = strlen(token);
    if (len < 8) {
        snprintf(s_tokval_error, sizeof(s_tokval_error), "%s", TRS("token vazio ou curto", "empty/short token"));
        s_tokval_state = TOKVAL_REJECTED;
        return false;
    }
    /* Erro comum: colar uma API key normal (sk-ant-api03-...) em vez do
     * token OAuth gerado por `claude setup-token` (sk-ant-oat01-...). Barra
     * aqui, sem gastar uma chamada de rede, com uma mensagem especifica. */
    if (strncmp(token, OAUTH_TOKEN_PREFIX, strlen(OAUTH_TOKEN_PREFIX)) != 0) {
        snprintf(s_tokval_error, sizeof(s_tokval_error),
                 TRS("deve comecar com %s (gere com: claude setup-token)",
                     "must start with %s (generate with: claude setup-token)"),
                 OAUTH_TOKEN_PREFIX);
        s_tokval_state = TOKVAL_REJECTED;
        return false;
    }
    s_tokval_state = TOKVAL_VALIDATING;

    UsageData tmp = {0};
    bool ok = fetchUsage(token, &tmp);
    if (ok) {
        snprintf(g_pendingToken, sizeof(g_pendingToken), "%s", token);
        g_usage = tmp;
        g_pinConfirming = false;
        g_pinFirst[0] = 0;
        g_pinEntry[0] = 0;
        s_tokval_state = TOKVAL_OK;
        g_tokenGot = true;
        return true;
    }
    snprintf(s_tokval_error, sizeof(s_tokval_error), "%s", tmp.error);
    s_tokval_state = TOKVAL_REJECTED;
    return false;
}

const char *onboarding_get_token_error(void)
{
    return s_tokval_error;
}

uint32_t onboarding_get_h5_reset_epoch(void)
{
    return g_usage.h5ResetEpoch;
}

void onboarding_on_tokens_pushed(long long tin, long long tout, long long cache, int sessions)
{
    g_tok.tin = tin;
    g_tok.tout = tout;
    g_tok.cache = cache;
    g_tok.sessions = sessions;
    g_tok.atMs = now_ms();
    ESP_LOGI(TAG, "tokens: in=%lld out=%lld cache=%lld sess=%d", tin, tout, cache, sessions);
    update_tok_row();
}

void ui_token_tick(void)
{
    if (!g_tokMsg) {
        return;
    }
    switch (s_tokval_state) {
    case TOKVAL_VALIDATING:
        lv_label_set_text(g_tokMsg, TRS("validando token...", "validating token..."));
        break;
    case TOKVAL_OK:
        lv_label_set_text(g_tokMsg, TRS("token OK! defina o PIN", "token OK! set the PIN"));
        s_tokval_state = TOKVAL_IDLE;
        break;
    case TOKVAL_REJECTED: {
        char m[96];
        snprintf(m, sizeof(m), TRS("token recusado (%s)", "token rejected (%s)"), s_tokval_error);
        lv_label_set_text(g_tokMsg, m);
        s_tokval_state = TOKVAL_IDLE;
        break;
    }
    default:
        break;
    }
}

void ui_token(void)
{
    lv_obj_t *scr = lv_screen_active();

    if (!g_onboarding && g_hasToken) {
        lv_obj_t *bk = mkbtn(scr, TRS(LV_SYMBOL_LEFT " Voltar", LV_SYMBOL_LEFT " Back"), &lv_font_montserrat_14,
                              C_SURFACE2, C_MUTED);
        lv_obj_align(bk, LV_ALIGN_TOP_RIGHT, -12, 8);
        lv_obj_add_event_cb(bk, nav_cb, LV_EVENT_CLICKED, (void *)(intptr_t)(g_usage.ok ? ST_MAIN : ST_SETTINGS));
    }

    lv_obj_t *mark = build_claude_mark(scr);
    lv_obj_align(mark, LV_ALIGN_TOP_MID, 0, 8);

    lv_obj_t *cap = mklabel(scr, TRS("Cole o token pelo navegador, em:", "Paste the token via browser, at:"),
                             &lv_font_montserrat_16, C_MUTED);
    lv_obj_align(cap, LV_ALIGN_TOP_MID, 0, 108);

    char url[48];
    snprintf(url, sizeof(url), "http://%s", wifi_manager_get_ip());
    lv_obj_t *ip = mklabel(scr, url, &lv_font_montserrat_28, C_ACCENT);
    lv_obj_align(ip, LV_ALIGN_TOP_MID, 0, 132);

    lv_obj_t *hint = mklabel(scr,
                              TRS("abra esse endereco no PC/celular na MESMA rede WiFi",
                                  "open this address on a PC/phone on the SAME WiFi"),
                              &lv_font_montserrat_12, C_MUTED);
    lv_obj_align(hint, LV_ALIGN_TOP_MID, 0, 174);

    lv_obj_t *sp = lv_spinner_create(scr);
    lv_spinner_set_anim_params(sp, 1200, 70);
    lv_obj_set_size(sp, 36, 36);
    lv_obj_align(sp, LV_ALIGN_BOTTOM_MID, 0, -46);
    lv_obj_set_style_arc_color(sp, lv_color_hex(C_SURFACE2), LV_PART_MAIN);
    lv_obj_set_style_arc_color(sp, lv_color_hex(C_ACCENT), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(sp, 5, LV_PART_MAIN);
    lv_obj_set_style_arc_width(sp, 5, LV_PART_INDICATOR);

    g_tokMsg = mklabel(scr, TRS("aguardando o token...", "waiting for the token..."), &lv_font_montserrat_14, C_MUTED);
    lv_obj_align(g_tokMsg, LV_ALIGN_BOTTOM_MID, 0, -14);
    s_tokval_state = TOKVAL_IDLE;

    ESP_LOGI(TAG, "onboarding web em %s", url);
}
