#include "app_state.h"

#include <stdio.h>

/* ============================================================
 * Tela: settings (lista rolavel; linhas >=44px de toque)
 * ============================================================ */
static bool g_wipeArmed = false;
static bool g_pinDisableArmed = false;
static lv_obj_t *g_briLbl = NULL, *g_wipeLbl = NULL, *g_pollLbl = NULL, *g_tzLbl = NULL, *g_slideLbl = NULL,
                *g_pinReqLbl = NULL;

static void pin_req_row_text(char *out, size_t sz)
{
    snprintf(out, sz, TRS(LV_SYMBOL_KEYBOARD "  Solicitar PIN no boot: %s", LV_SYMBOL_KEYBOARD "  Ask for PIN at boot: %s"),
             g_pinRequired ? TRS("Sim", "Yes") : TRS("Nao", "No"));
}
static const int POLL_OPTS[4] = {30, 60, 120, 300};
static const int TZ_OPTS[] = {-3, -4, -5, -6, -7, -8, -2, -1, 0, 1, 2, 3};
#define NTZ ((int)(sizeof(TZ_OPTS) / sizeof(TZ_OPTS[0])))

static void settings_action_cb(lv_event_t *e)
{
    int act = (int)(intptr_t)lv_event_get_user_data(e);
    switch (act) {
    case 0:
        request_state(ST_LOADING);
        break;
    case 1:
        g_onboarding = false;
        request_state(ST_WIFI);
        break;
    case 2:
        request_state(ST_TOKEN);
        break;
    case 3: {
        g_briIdx = (g_briIdx + 1) % 3;
        apply_brightness();
        if (g_briLbl) {
            const char *n[3] = {TRS("baixo", "low"), TRS("medio", "medium"), TRS("alto", "high")};
            char m[40];
            snprintf(m, sizeof(m), TRS(LV_SYMBOL_EYE_OPEN "  Brilho: %s", LV_SYMBOL_EYE_OPEN "  Brightness: %s"),
                     n[g_briIdx]);
            lv_label_set_text(g_briLbl, m);
        }
        break;
    }
    case 4:
        if (!g_wipeArmed) {
            g_wipeArmed = true;
            if (g_wipeLbl) {
                lv_label_set_text(g_wipeLbl, TRS(LV_SYMBOL_TRASH "  Toque de novo p/ confirmar",
                                                  LV_SYMBOL_TRASH "  Tap again to confirm"));
            }
        } else {
            g_wipeArmed = false;
            factory_reset();
            request_state(ST_WIFI);
        }
        break;
    case 5:
        request_state(ST_MAIN);
        break;
    case 6: {
        int idx = 0;
        for (int i = 0; i < 4; i++) {
            if (POLL_OPTS[i] == g_pollSec) {
                idx = i;
            }
        }
        g_pollSec = POLL_OPTS[(idx + 1) % 4];
        save_settings();
        if (g_pollLbl) {
            char m[40];
            if (g_pollSec < 60) {
                snprintf(m, sizeof(m), TRS(LV_SYMBOL_LOOP "  Atualizar: %ds", LV_SYMBOL_LOOP "  Refresh: %ds"),
                         g_pollSec);
            } else {
                snprintf(m, sizeof(m), TRS(LV_SYMBOL_LOOP "  Atualizar: %dmin", LV_SYMBOL_LOOP "  Refresh: %dmin"),
                         g_pollSec / 60);
            }
            lv_label_set_text(g_pollLbl, m);
        }
        break;
    }
    case 7: {
        int idx = 0;
        for (int i = 0; i < NTZ; i++) {
            if (TZ_OPTS[i] == g_tzOffset) {
                idx = i;
            }
        }
        g_tzOffset = TZ_OPTS[(idx + 1) % NTZ];
        apply_tz();
        if (g_tzLbl) {
            char m[40];
            snprintf(m, sizeof(m), TRS(LV_SYMBOL_GPS "  Fuso: GMT%+d", LV_SYMBOL_GPS "  Timezone: GMT%+d"), g_tzOffset);
            lv_label_set_text(g_tzLbl, m);
        }
        break;
    }
    case 8: {
        static const int SL[5] = {0, 5, 10, 15, 30};
        int idx = 0;
        for (int i = 0; i < 5; i++) {
            if (SL[i] == g_slideSec) {
                idx = i;
            }
        }
        g_slideSec = SL[(idx + 1) % 5];
        save_settings();
        if (g_slideLbl) {
            char m[48];
            if (g_slideSec) {
                snprintf(m, sizeof(m), LV_SYMBOL_PLAY "  Slideshow: %ds", g_slideSec);
            } else {
                snprintf(m, sizeof(m), "%s", TRS(LV_SYMBOL_PLAY "  Slideshow: desligado", LV_SYMBOL_PLAY "  Slideshow: off"));
            }
            lv_label_set_text(g_slideLbl, m);
        }
        break;
    }
    case 9:
        g_lang ^= 1;
        save_settings();
        request_state(ST_SETTINGS);
        break;
    case 10:
        request_state(ST_ABOUT);
        break;
    case 12:
        request_state(ST_NTP_SERVER);
        break;
    case 11:
        if (g_pinRequired) {
            /* Desligar: precisa de 2 toques (reduz a protecao do token). */
            if (!g_pinDisableArmed) {
                g_pinDisableArmed = true;
                if (g_pinReqLbl) {
                    lv_label_set_text(g_pinReqLbl,
                                       TRS(LV_SYMBOL_WARNING "  Toque de novo p/ reduzir a seguranca",
                                           LV_SYMBOL_WARNING "  Tap again to reduce security"));
                }
                break;
            }
            g_pinDisableArmed = false;
            if (encryptToken(g_token, FIXED_PIN, &g_blob)) {
                save_blob();
                g_pinRequired = false;
                save_settings();
            }
            if (g_pinReqLbl) {
                char m[48];
                pin_req_row_text(m, sizeof(m));
                lv_label_set_text(g_pinReqLbl, m);
            }
        } else {
            /* Ligar: precisa definir um PIN de verdade primeiro. */
            g_pinDisableArmed = false;
            g_pinSetupFromSettings = true;
            request_state(ST_SETUP_PIN);
        }
        break;
    default:
        break;
    }
}

static void add_setting_row(lv_obj_t *p, const char *txt, int act, uint32_t fg, lv_obj_t **out)
{
    lv_obj_t *b = lv_button_create(p);
    lv_obj_set_size(b, 444, 44);
    lv_obj_set_style_bg_color(b, lv_color_hex(C_SURFACE), 0);
    lv_obj_set_style_radius(b, 12, 0);
    lv_obj_set_style_shadow_width(b, 0, 0);
    lv_obj_t *l = mklabel(b, txt, &lv_font_montserrat_16, fg);
    lv_obj_align(l, LV_ALIGN_LEFT_MID, 8, 0);
    lv_obj_add_event_cb(b, settings_action_cb, LV_EVENT_CLICKED, (void *)(intptr_t)act);
    if (out) {
        *out = l;
    }
}

void ui_settings(void)
{
    lv_obj_t *scr = lv_screen_active();
    g_wipeArmed = false;
    g_pinDisableArmed = false;
    lv_obj_t *title = mklabel(scr, TRS("Ajustes", "Settings"), &lv_font_montserrat_20, C_TEXT);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 14, 10);

    lv_obj_t *bk = mkbtn(scr, TRS(LV_SYMBOL_LEFT " Voltar", LV_SYMBOL_LEFT " Back"), &lv_font_montserrat_14,
                          C_SURFACE2, C_MUTED);
    lv_obj_set_size(bk, 100, 32);
    lv_obj_set_ext_click_area(bk, 6);
    lv_obj_align(bk, LV_ALIGN_TOP_RIGHT, -12, 6);
    lv_obj_add_event_cb(bk, nav_cb, LV_EVENT_CLICKED, (void *)(intptr_t)(g_usage.ok ? ST_MAIN : ST_SETTINGS));

    lv_obj_t *lst = lv_obj_create(scr);
    lv_obj_set_pos(lst, 8, 44);
    lv_obj_set_size(lst, 464, 268);
    lv_obj_set_style_bg_opa(lst, 0, 0);
    lv_obj_set_style_border_width(lst, 0, 0);
    lv_obj_set_style_pad_all(lst, 0, 0);
    lv_obj_set_style_pad_row(lst, 8, 0);
    lv_obj_set_flex_flow(lst, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(lst, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(lst, LV_SCROLLBAR_MODE_AUTO);

    const char *n[3] = {TRS("baixo", "low"), TRS("medio", "medium"), TRS("alto", "high")};
    char bri[40];
    snprintf(bri, sizeof(bri), TRS(LV_SYMBOL_EYE_OPEN "  Brilho: %s", LV_SYMBOL_EYE_OPEN "  Brightness: %s"),
             n[g_briIdx]);
    char pollTxt[40];
    if (g_pollSec < 60) {
        snprintf(pollTxt, sizeof(pollTxt), TRS(LV_SYMBOL_LOOP "  Atualizar: %ds", LV_SYMBOL_LOOP "  Refresh: %ds"),
                 g_pollSec);
    } else {
        snprintf(pollTxt, sizeof(pollTxt), TRS(LV_SYMBOL_LOOP "  Atualizar: %dmin", LV_SYMBOL_LOOP "  Refresh: %dmin"),
                 g_pollSec / 60);
    }
    char tzTxt[40];
    snprintf(tzTxt, sizeof(tzTxt), TRS(LV_SYMBOL_GPS "  Fuso: GMT%+d", LV_SYMBOL_GPS "  Timezone: GMT%+d"), g_tzOffset);
    char slideTxt[48];
    if (g_slideSec) {
        snprintf(slideTxt, sizeof(slideTxt), LV_SYMBOL_PLAY "  Slideshow: %ds", g_slideSec);
    } else {
        snprintf(slideTxt, sizeof(slideTxt), "%s", TRS(LV_SYMBOL_PLAY "  Slideshow: desligado", LV_SYMBOL_PLAY "  Slideshow: off"));
    }
    char pinReqTxt[48];
    pin_req_row_text(pinReqTxt, sizeof(pinReqTxt));
    char ntpTxt[96];
    snprintf(ntpTxt, sizeof(ntpTxt), TRS(LV_SYMBOL_GPS "  Servidor NTP: %s", LV_SYMBOL_GPS "  NTP server: %s"),
             g_ntpServer);

    add_setting_row(lst, TRS(LV_SYMBOL_REFRESH "  Atualizar agora", LV_SYMBOL_REFRESH "  Refresh now"), 0, C_TEXT, NULL);
    add_setting_row(lst, pollTxt, 6, C_TEXT, &g_pollLbl);
    add_setting_row(lst, slideTxt, 8, C_TEXT, &g_slideLbl);
    add_setting_row(lst, TRS(LV_SYMBOL_LIST "  Idioma: Portugues", LV_SYMBOL_LIST "  Language: English"), 9, C_TEXT, NULL);
    add_setting_row(lst, tzTxt, 7, C_TEXT, &g_tzLbl);
    add_setting_row(lst, bri, 3, C_TEXT, &g_briLbl);
    add_setting_row(lst, TRS(LV_SYMBOL_WIFI "  Configurar WiFi", LV_SYMBOL_WIFI "  Configure WiFi"), 1, C_TEXT, NULL);
    add_setting_row(lst, TRS(LV_SYMBOL_KEYBOARD "  Trocar token", LV_SYMBOL_KEYBOARD "  Change token"), 2, C_TEXT, NULL);
    add_setting_row(lst, pinReqTxt, 11, C_TEXT, &g_pinReqLbl);
    add_setting_row(lst, ntpTxt, 12, C_TEXT, NULL);
    add_setting_row(lst, TRS(LV_SYMBOL_FILE "  Sobre", LV_SYMBOL_FILE "  About"), 10, C_TEXT, NULL);
    add_setting_row(lst, TRS(LV_SYMBOL_TRASH "  Apagar tudo", LV_SYMBOL_TRASH "  Erase everything"), 4, C_BAD, &g_wipeLbl);
}

/* ============================================================
 * Tela: sobre / about
 * ============================================================ */
void ui_about(void)
{
    lv_obj_t *scr = lv_screen_active();

    lv_obj_t *bk = mkbtn(scr, TRS(LV_SYMBOL_LEFT " Voltar", LV_SYMBOL_LEFT " Back"), &lv_font_montserrat_14,
                          C_SURFACE2, C_MUTED);
    lv_obj_set_size(bk, 100, 32);
    lv_obj_set_ext_click_area(bk, 6);
    lv_obj_align(bk, LV_ALIGN_TOP_RIGHT, -12, 6);
    lv_obj_add_event_cb(bk, nav_cb, LV_EVENT_CLICKED, (void *)(intptr_t)ST_SETTINGS);

    lv_obj_t *mark = build_claude_mark(scr);
    lv_obj_align(mark, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t *t = mklabel(scr, "Claude Usage Stick", &lv_font_montserrat_22, C_TEXT);
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 94);

    char v[80];
    snprintf(v, sizeof(v), "v" FW_VERSION " \xE2\x80\xA2 ESP32-S3 \xE2\x80\xA2 LVGL 9.2 \xE2\x80\xA2 Panlee SC01 Plus");
    lv_obj_t *ver = mklabel(scr, v, &lv_font_montserrat_12, C_FAINT);
    lv_obj_align(ver, LV_ALIGN_TOP_MID, 0, 122);

    lv_obj_t *d = mklabel(scr,
                           TRS("Medidor de uso do Claude Code em tempo real: "
                               "janelas de 5h e semanal direto da API da Anthropic.",
                               "Real-time Claude Code usage meter: "
                               "5-hour and weekly windows straight from the Anthropic API."),
                           &lv_font_montserrat_14, C_MUTED);
    lv_obj_set_width(d, 420);
    lv_obj_set_style_text_align(d, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(d, LV_LABEL_LONG_WRAP);
    lv_obj_align(d, LV_ALIGN_TOP_MID, 0, 146);

    lv_obj_t *h = mklabel(scr,
                           TRS("Tela: Smart Panlee SC01 Plus \xE2\x80\xA2 IPS 3.5\" 480x320 touch (ST7796UI/FT6336U)",
                               "Display: Smart Panlee SC01 Plus \xE2\x80\xA2 3.5\" IPS 480x320 touch (ST7796UI/FT6336U)"),
                           &lv_font_montserrat_12, C_FAINT);
    lv_obj_align(h, LV_ALIGN_TOP_MID, 0, 200);

    lv_obj_t *devCap = mklabel(scr, TRS("Baseado no projeto original de", "Based on the original project by"),
                                &lv_font_montserrat_12, C_FAINT);
    lv_obj_align(devCap, LV_ALIGN_TOP_MID, 0, 224);
    lv_obj_t *dev = mklabel(scr, "Benevid Felix", &lv_font_montserrat_18, C_TEXT);
    lv_obj_align(dev, LV_ALIGN_TOP_MID, 0, 244);

    lv_obj_t *portCap = mklabel(scr,
                                 TRS("Adaptado para a Panlee SC01 Plus por", "Ported to the Panlee SC01 Plus by"),
                                 &lv_font_montserrat_12, C_FAINT);
    lv_obj_align(portCap, LV_ALIGN_TOP_MID, 0, 272);
    lv_obj_t *portDev = mklabel(scr, "Marcelo Sobral", &lv_font_montserrat_16, C_TEXT);
    lv_obj_align(portDev, LV_ALIGN_TOP_MID, 0, 292);
}

/* ============================================================
 * Tela: Servidor NTP (editavel; so aplicado no PROXIMO boot, pois
 * time_sync_start() roda uma unica vez por sessao - ver time_sync.c)
 * ============================================================ */
static lv_obj_t *ntp_ta = NULL, *ntp_status = NULL;

static void ntp_back_cb(lv_event_t *e)
{
    (void)e;
    request_state(ST_SETTINGS);
}

static void ntp_kb_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_READY) {
        const char *val = lv_textarea_get_text(ntp_ta);
        if (val[0]) {
            snprintf(g_ntpServer, sizeof(g_ntpServer), "%s", val);
            storage_save_ntp_server(g_ntpServer);
            lv_label_set_text(ntp_status,
                               TRS("Salvo. Sera aplicado no proximo boot.", "Saved. Applied on next boot."));
        } else {
            lv_label_set_text(ntp_status, TRS("Digite um endereco valido.", "Type a valid address."));
        }
    } else if (code == LV_EVENT_CANCEL) {
        request_state(ST_SETTINGS);
    }
}

void ui_ntp_server(void)
{
    lv_obj_t *scr = lv_screen_active();
    lv_obj_t *title = mklabel(scr, TRS("Servidor NTP", "NTP server"), &lv_font_montserrat_20, C_TEXT);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 14, 12);

    lv_obj_t *bk = mkbtn(scr, TRS(LV_SYMBOL_LEFT " Voltar", LV_SYMBOL_LEFT " Back"), &lv_font_montserrat_14,
                          C_SURFACE2, C_MUTED);
    lv_obj_set_size(bk, 100, 32);
    lv_obj_set_ext_click_area(bk, 6);
    lv_obj_align(bk, LV_ALIGN_TOP_RIGHT, -12, 6);
    lv_obj_add_event_cb(bk, ntp_back_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *hint = mklabel(scr,
                              TRS("Nome ou IP do servidor NTP primario (ex.: pool.ntp.br). "
                                  "Secundario fixo: time.cloudflare.com.",
                                  "Hostname or IP of the primary NTP server (e.g. pool.ntp.br). "
                                  "Fixed secondary: time.cloudflare.com."),
                              &lv_font_montserrat_12, C_MUTED);
    lv_obj_set_width(hint, 452);
    lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);
    lv_obj_align(hint, LV_ALIGN_TOP_MID, 0, 44);

    ntp_ta = lv_textarea_create(scr);
    lv_textarea_set_one_line(ntp_ta, true);
    lv_textarea_set_max_length(ntp_ta, NTP_SERVER_MAX_LEN - 1);
    lv_textarea_set_text(ntp_ta, g_ntpServer);
    lv_obj_set_size(ntp_ta, 452, 44);
    lv_obj_align(ntp_ta, LV_ALIGN_TOP_MID, 0, 88);

    ntp_status = mklabel(scr, TRS("Toque em OK no teclado para salvar.", "Tap OK on the keyboard to save."),
                          &lv_font_montserrat_12, C_MUTED);
    lv_obj_align(ntp_status, LV_ALIGN_TOP_MID, 0, 136);

    lv_obj_t *kb = lv_keyboard_create(scr);
    lv_keyboard_set_textarea(kb, ntp_ta);
    lv_obj_add_event_cb(kb, ntp_kb_cb, LV_EVENT_ALL, NULL);
}
