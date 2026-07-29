#include "app_state.h"
#include "logo_assets.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

/* ============================================================
 * Mascotes (pagina de Modelos)
 * ============================================================ */
int model_mood(int i)
{
    bool inc = (i == 0) ? g_status.haikuUp : (i == 1) ? g_status.sonnetUp : (i == 2) ? g_status.opusUp : g_status.fableUp;
    int c = g_models[i].pr.code;
    if (!inc) {
        return 3;
    }
    if (c == 0) {
        return 0;
    }
    if (c == 200) {
        return 1;
    }
    if (c == 429) {
        return 2;
    }
    if (c == 404) {
        return 4;
    }
    return 3;
}

static void build_accessory(lv_obj_t *c, int model)
{
    switch (model) {
    case 0:
        rrect(c, 46, 0, 8, 7, 1, C_WARN);
        rrect(c, 41, 5, 8, 7, 1, C_WARN);
        rrect(c, 46, 10, 8, 7, 1, C_WARN);
        break;
    case 1:
        rrect(c, 50, 0, 10, 4, 1, 0x7DD3FC);
        rrect(c, 50, 0, 4, 13, 1, 0x7DD3FC);
        rrect(c, 44, 10, 8, 7, 3, 0x7DD3FC);
        break;
    case 2:
        rrect(c, 30, 4, 7, 8, 1, C_WARN);
        rrect(c, 41, 1, 7, 11, 1, C_WARN);
        rrect(c, 52, 4, 7, 8, 1, C_WARN);
        rrect(c, 30, 12, 29, 6, 1, C_WARN);
        break;
    case 3:
        rrect(c, 41, 0, 6, 17, 2, 0xC4B5FD);
        rrect(c, 36, 6, 16, 6, 2, 0xC4B5FD);
        break;
    default:
        break;
    }
}

static void build_model_mascot(lv_obj_t *parent, int cx, int i)
{
    if (g_mascN >= NMODELS) {
        return;
    }
    int mood = model_mood(i);
    int baseY = 14;
    lv_obj_t *c = lv_obj_create(parent);
    lv_obj_set_pos(c, cx - 44, baseY);
    lv_obj_set_size(c, 88, 80);
    no_box(c);

    lv_obj_t *img = lv_image_create(c);
    lv_image_set_src(img, &img_clawd_md);
    lv_obj_set_pos(img, 0, 20);

    build_accessory(c, i);

    const int ex[2] = {CLAWD_MD_EYE0_X, CLAWD_MD_EYE1_X};
    const int ey = CLAWD_MD_EYE0_Y + 20, ew = CLAWD_MD_EYE0_W, eh = CLAWD_MD_EYE0_H;

    Mascot *m = &g_masc[g_mascN];
    m->cont = c;
    m->img = img;
    m->baseY = baseY;
    m->mood = mood;
    m->lid[0] = m->lid[1] = NULL;
    m->drop = NULL;

    if (mood == 1) {
        for (int k = 0; k < 2; k++) {
            m->lid[k] = rrect(c, ex[k] - 1, ey - 1, ew + 2, eh + 2, 1, C_ACCENT);
            lv_obj_add_flag(m->lid[k], LV_OBJ_FLAG_HIDDEN);
        }
    } else if (mood == 2) {
        m->drop = rrect(c, 70, 24, 6, 10, 3, 0x7DD3FC);
    } else if (mood == 3) {
        lv_obj_set_style_image_recolor(img, lv_color_hex(0x6A6A74), 0);
        lv_obj_set_style_image_recolor_opa(img, 190, 0);
        lv_obj_set_y(img, 24);
        for (int k = 0; k < 2; k++) {
            g_mXPts[i][k * 2][0] = (lv_point_precise_t){(lv_value_precise_t)(ex[k] - 2), (lv_value_precise_t)(ey + 2)};
            g_mXPts[i][k * 2][1] = (lv_point_precise_t){(lv_value_precise_t)(ex[k] + ew + 2), (lv_value_precise_t)(ey + eh + 6)};
            g_mXPts[i][k * 2 + 1][0] = (lv_point_precise_t){(lv_value_precise_t)(ex[k] + ew + 2), (lv_value_precise_t)(ey + 2)};
            g_mXPts[i][k * 2 + 1][1] = (lv_point_precise_t){(lv_value_precise_t)(ex[k] - 2), (lv_value_precise_t)(ey + eh + 6)};
            for (int l = 0; l < 2; l++) {
                lv_obj_t *ln = lv_line_create(c);
                lv_line_set_points(ln, g_mXPts[i][k * 2 + l], 2);
                lv_obj_set_style_line_width(ln, 3, 0);
                lv_obj_set_style_line_color(ln, lv_color_hex(C_BAD), 0);
                lv_obj_set_style_line_rounded(ln, true, 0);
            }
        }
    } else if (mood == 4) {
        lv_obj_set_style_image_recolor(img, lv_color_hex(0x6A6A74), 0);
        lv_obj_set_style_image_recolor_opa(img, 170, 0);
        for (int k = 0; k < 2; k++) {
            m->lid[k] = rrect(c, ex[k] - 1, ey + eh / 2, ew + 2, eh / 2 + 1, 1, 0x8A8A94);
        }
        lv_obj_set_style_opa(c, 180, 0);
    } else {
        lv_obj_set_style_opa(c, 140, 0);
    }
    g_mascN++;
}

static void model_chip(int i, char *out, size_t sz, uint32_t *col)
{
    int c = g_models[i].pr.code;
    if (c == 0) {
        snprintf(out, sz, "--");
        *col = C_MUTED;
    } else if (c == 200) {
        snprintf(out, sz, "OK %.1fs", g_models[i].pr.ms / 1000.0f);
        *col = C_OK;
    } else if (c == 429) {
        snprintf(out, sz, "%s", TRS("LIMITADO", "LIMITED"));
        *col = C_WARN;
    } else if (c == 404) {
        snprintf(out, sz, "%s", TRS("N/D", "N/A"));
        *col = C_MUTED;
    } else if (c == 401 || c == 403) {
        snprintf(out, sz, "AUTH");
        *col = C_BAD;
    } else if (c < 0) {
        snprintf(out, sz, "%s", TRS("REDE", "NET"));
        *col = C_BAD;
    } else {
        snprintf(out, sz, TRS("ERRO %d", "ERR %d"), c);
        *col = C_BAD;
    }
}

/* ============================================================
 * Builders dos 4 tiles
 * ============================================================ */
static void build_win_card(lv_obj_t *t, int x, const char *title, lv_obj_t **pct, lv_obj_t **seg, lv_obj_t **at,
                            lv_obj_t **cd)
{
    lv_obj_t *c = card(t, x, 4, 228, 210);
    tstatic(c, title, &lv_font_montserrat_14, C_MUTED, 0, 0);
    *pct = tlabel(c, &lv_font_montserrat_48, C_OK, 0, 20);
    for (int i = 0; i < NSEG; i++) {
        seg[i] = rrect(c, i * 11, 82, 8, 16, 2, C_TRACK);
    }
    *at = tlabel(c, &lv_font_montserrat_12, C_FAINT, 0, 106);
    *cd = tlabel(c, &lv_font_montserrat_40, C_TEXT, 0, 124);
}

static void build_tile_agora(lv_obj_t *t)
{
    build_win_card(t, 8, TRS("5 HORAS", "5 HOURS"), &g_ui.agPct5, g_ui.seg5, &g_ui.agAt5, &g_ui.agCd5);
    build_win_card(t, 244, TRS("SEMANA", "WEEK"), &g_ui.agPct7, g_ui.seg7, &g_ui.agAt7, &g_ui.agCd7);
    g_ui.agChip = mkchip(t, 8, 220);
    g_ui.agTok = tlabel(t, &lv_font_montserrat_12, C_MUTED, 130, 226);
    lv_obj_set_width(g_ui.agTok, 342);
    lv_obj_set_style_text_align(g_ui.agTok, LV_TEXT_ALIGN_RIGHT, 0);
}

static const int MODEL_CENTERS[NMODELS] = {60, 180, 300, 420};

static void build_tile_models(lv_obj_t *t)
{
    for (int i = 0; i < NMODELS; i++) {
        build_model_mascot(t, MODEL_CENTERS[i], i);
        lv_obj_t *n = mklabel(t, g_models[i].name, &lv_font_montserrat_16, model_mood(i) == 1 ? C_TEXT : C_MUTED);
        lv_obj_set_width(n, 104);
        lv_obj_set_style_text_align(n, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(n, MODEL_CENTERS[i] - 52, 98);
        g_ui.mChip[i] = mkchip(t, 0, 122);
    }
    tstatic(t, TRS("sonda real na API \xE2\x80\xA2 1 modelo por ciclo", "live API probe \xE2\x80\xA2 1 model per cycle"),
            &lv_font_montserrat_12, C_FAINT, 14, 170);
    g_ui.incident = tlabel(t, &lv_font_montserrat_14, C_MUTED, 14, 194);
    lv_obj_set_width(g_ui.incident, 452);
    lv_label_set_long_mode(g_ui.incident, LV_LABEL_LONG_WRAP);
}

#define TR_X0 12
#define TR_Y0 10
#define TR_W 440
#define TR_H 126

static int tr_x(uint32_t tt, uint32_t ws, uint32_t we)
{
    if (we <= ws) {
        return TR_X0;
    }
    long long v = (long long)(tt - ws) * TR_W / (long long)(we - ws);
    if (v < 0) {
        v = 0;
    }
    if (v > TR_W) {
        v = TR_W;
    }
    return TR_X0 + (int)v;
}

static int tr_y(float p)
{
    if (p < 0) {
        p = 0;
    }
    if (p > 100) {
        p = 100;
    }
    return TR_Y0 + TR_H - (int)(p * TR_H / 100.0f);
}

static void build_tile_trend(lv_obj_t *t)
{
    tstatic(t, TRS("Janela de 5h", "5-hour window"), &lv_font_montserrat_16, C_TEXT, 14, 2);
    tstatic(t, TRS("uso real + projecao", "real usage + projection"), &lv_font_montserrat_12, C_FAINT, 320, 6);

    lv_obj_t *c = card(t, 8, 26, 464, 170);
    lv_obj_set_style_pad_all(c, 0, 0);

    for (int i = 1; i <= 3; i++) {
        rrect(c, TR_X0, tr_y(i * 25.0f), TR_W, 1, 0, C_GRID);
    }
    tstatic(c, "100", &lv_font_montserrat_12, C_FAINT, TR_X0 + TR_W - 24, TR_Y0 - 6);
    tstatic(c, "0", &lv_font_montserrat_12, C_FAINT, TR_X0 + TR_W - 10, TR_Y0 + TR_H - 14);

    g_ui.trHist = lv_line_create(c);
    lv_obj_set_pos(g_ui.trHist, 0, 0);
    lv_obj_set_style_line_width(g_ui.trHist, 3, 0);
    lv_obj_set_style_line_color(g_ui.trHist, lv_color_hex(C_ACCENT), 0);
    lv_obj_set_style_line_rounded(g_ui.trHist, true, 0);

    g_ui.trProj = lv_line_create(c);
    lv_obj_set_pos(g_ui.trProj, 0, 0);
    lv_obj_set_style_line_width(g_ui.trProj, 2, 0);
    lv_obj_set_style_line_color(g_ui.trProj, lv_color_hex(C_ACCENT), 0);
    lv_obj_set_style_line_opa(g_ui.trProj, 170, 0);
    lv_obj_set_style_line_dash_width(g_ui.trProj, 6, 0);
    lv_obj_set_style_line_dash_gap(g_ui.trProj, 6, 0);

    g_ui.trDot = rrect(c, 0, 0, 8, 8, 4, C_TEXT);
    lv_obj_add_flag(g_ui.trDot, LV_OBJ_FLAG_HIDDEN);

    g_ui.trT0 = tlabel(c, &lv_font_montserrat_12, C_FAINT, TR_X0, TR_Y0 + TR_H + 8);
    g_ui.trT1 = tlabel(c, &lv_font_montserrat_12, C_FAINT, TR_X0 + TR_W - 40, TR_Y0 + TR_H + 8);

    g_ui.trCap = tlabel(t, &lv_font_montserrat_16, C_MUTED, 14, 210);
    lv_obj_set_width(g_ui.trCap, 452);
    lv_label_set_long_mode(g_ui.trCap, LV_LABEL_LONG_WRAP);
}

static void heat_btn_style(void)
{
    const char *names[4] = {TRS("Hoje", "Today"), "7d", "30d", TRS("Tudo", "All")};
    for (int i = 0; i < 4; i++) {
        if (!g_ui.heatBtn[i]) {
            continue;
        }
        bool on = (i == g_heatMode);
        lv_obj_set_style_bg_color(g_ui.heatBtn[i], lv_color_hex(on ? C_ACCENT : C_SURFACE2), 0);
        lv_obj_t *l = lv_obj_get_child(g_ui.heatBtn[i], 0);
        if (l) {
            lv_label_set_text(l, names[i]);
            lv_obj_set_style_text_color(l, lv_color_hex(on ? C_BG : C_MUTED), 0);
        }
    }
}

static void heat_btn_cb(lv_event_t *e)
{
    int m = (int)(intptr_t)lv_event_get_user_data(e);
    if (m == g_heatMode) {
        return;
    }
    g_heatMode = m;
    save_settings();
    heat_btn_style();
    heat_redraw();
}

static void build_tile_heat(lv_obj_t *t)
{
    tstatic(t, TRS("Ritmo por hora", "Hourly rhythm"), &lv_font_montserrat_16, C_TEXT, 14, 6);
    for (int i = 0; i < 4; i++) {
        lv_obj_t *b = lv_button_create(t);
        lv_obj_set_size(b, 52, 30);
        lv_obj_set_pos(b, 246 + i * 56, 0);
        lv_obj_set_style_radius(b, 15, 0);
        lv_obj_set_style_shadow_width(b, 0, 0);
        lv_obj_set_ext_click_area(b, 6);
        lv_obj_t *l = mklabel(b, "", &lv_font_montserrat_14, C_MUTED);
        lv_obj_center(l);
        lv_obj_add_event_cb(b, heat_btn_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        g_ui.heatBtn[i] = b;
    }
    heat_btn_style();
    for (int h = 0; h < 24; h++) {
        lv_obj_t *bar = lv_obj_create(t);
        lv_obj_set_size(bar, 13, 4);
        lv_obj_set_pos(bar, 18 + h * 18, 176);
        lv_obj_set_style_radius(bar, 3, 0);
        lv_obj_set_style_bg_color(bar, lv_color_hex(C_ACCENT), 0);
        lv_obj_set_style_border_width(bar, 0, 0);
        lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
        g_ui.heat[h] = bar;
    }
    int ticks[5] = {0, 6, 12, 18, 23};
    for (int i = 0; i < 5; i++) {
        int h = ticks[i];
        char s[4];
        snprintf(s, sizeof(s), "%dh", h);
        lv_obj_t *l = mklabel(t, s, &lv_font_montserrat_12, C_MUTED);
        lv_obj_set_pos(l, 14 + h * 18, 186);
    }
    tstatic(t, TRS("quota da janela 5h queimada em cada hora local", "5h-window quota burned per local hour"),
            &lv_font_montserrat_12, C_FAINT, 14, 214);
}

static void on_tile_changed(lv_event_t *e)
{
    (void)e;
    if (!g_ui.tv) {
        return;
    }
    lv_obj_t *act = lv_tileview_get_tile_active(g_ui.tv);
    for (int i = 0; i < NTILES; i++) {
        if (!g_ui.dots[i]) {
            continue;
        }
        bool on = (g_ui.tile[i] == act);
        if (on) {
            g_curTile = i;
        }
        lv_obj_set_style_bg_color(g_ui.dots[i], lv_color_hex(on ? C_ACCENT : C_BORDER), 0);
        lv_obj_set_width(g_ui.dots[i], on ? 18 : 8);
    }
}

/* ============================================================
 * Atualizacao de valores
 * ============================================================ */
void update_tok_row(void)
{
    if (!g_ui.agTok) {
        return;
    }
    if (g_tok.atMs == 0 || now_ms() - g_tok.atMs > TOK_FRESH_MS) {
        lv_label_set_text(g_ui.agTok, "");
        return;
    }
    char a[16], b[16], s[96];
    fmt_tok(g_tok.tin, a, sizeof(a));
    fmt_tok(g_tok.tout, b, sizeof(b));
    snprintf(s, sizeof(s), TRS("tokens na janela: %s entrada \xE2\x80\xA2 %s saida", "window tokens: %s in \xE2\x80\xA2 %s out"),
             a, b);
    lv_label_set_text(g_ui.agTok, s);
}

void dash_tick(void)
{
    if (g_state != ST_MAIN || !g_ui.agCd5) {
        return;
    }
    char e[32], c[24], b[64];
    fmt_eta(g_usage.h5ResetEpoch, e, sizeof(e));
    lv_label_set_text(g_ui.agCd5, e);
    fmt_clock(g_usage.h5ResetEpoch, c, sizeof(c));
    snprintf(b, sizeof(b), TRS("RESETA EM \xE2\x80\xA2 %s", "RESETS \xE2\x80\xA2 %s"), c);
    lv_label_set_text(g_ui.agAt5, b);

    fmt_eta(g_usage.d7ResetEpoch, e, sizeof(e));
    lv_label_set_text(g_ui.agCd7, e);
    fmt_clock(g_usage.d7ResetEpoch, c, sizeof(c));
    snprintf(b, sizeof(b), TRS("RESETA EM \xE2\x80\xA2 %s", "RESETS \xE2\x80\xA2 %s"), c);
    lv_label_set_text(g_ui.agAt7, b);

    set_hdr_status();
}

void trend_redraw(void)
{
    if (!g_ui.trHist) {
        return;
    }
    time_t now = time(NULL);
    uint32_t we = g_usage.h5ResetEpoch;
    bool clockOk = (now > 1000000000L) && we != 0;

    if (!clockOk) {
        lv_line_set_points(g_ui.trHist, g_trPts, 0);
        lv_line_set_points(g_ui.trProj, g_trProjPts, 0);
        lv_obj_add_flag(g_ui.trDot, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(g_ui.trCap, TRS("Aguardando dados da janela...", "Waiting for window data..."));
        lv_obj_set_style_text_color(g_ui.trCap, lv_color_hex(C_MUTED), 0);
        return;
    }
    uint32_t ws = we - 5 * 3600;

    char t0[12], t1[12], b[112];
    fmt_hm(ws, t0, sizeof(t0));
    fmt_hm(we, t1, sizeof(t1));
    lv_label_set_text(g_ui.trT0, t0);
    lv_label_set_text(g_ui.trT1, t1);

    int n = 0;
    for (int i = 0; i < g_histN && n < HIST_MAX; i++) {
        Sample s = g_hist[hist_idx(i)];
        if (s.t == 0 || s.t < ws || s.t > (uint32_t)now) {
            continue;
        }
        g_trPts[n].x = tr_x(s.t, ws, we);
        g_trPts[n].y = tr_y(s.h5);
        n++;
    }
    uint32_t nowClamped = ((uint32_t)now > we) ? we : (uint32_t)now;
    if (n < HIST_MAX) {
        g_trPts[n].x = tr_x(nowClamped, ws, we);
        g_trPts[n].y = tr_y(g_usage.h5);
        n++;
    }
    lv_line_set_points(g_ui.trHist, g_trPts, n);

    int cx = tr_x(nowClamped, ws, we), cy = tr_y(g_usage.h5);
    lv_obj_set_pos(g_ui.trDot, cx - 4, cy - 4);
    lv_obj_clear_flag(g_ui.trDot, LV_OBJ_FLAG_HIDDEN);

    if (n < 3) {
        lv_line_set_points(g_ui.trProj, g_trProjPts, 0);
        lv_label_set_text(g_ui.trCap, TRS("Coletando dados... (~alguns minutos)", "Collecting data... (~a few minutes)"));
        lv_obj_set_style_text_color(g_ui.trCap, lv_color_hex(C_MUTED), 0);
        return;
    }

    float rate = 0;
    {
        Sample first = {0, 0, 0};
        for (int i = 0; i < g_histN; i++) {
            Sample s = g_hist[hist_idx(i)];
            if (s.t == 0 || s.t < ws) {
                continue;
            }
            if (s.t >= (uint32_t)now - 2700) {
                first = s;
                break;
            }
        }
        if (first.t != 0 && (uint32_t)now > first.t + 300) {
            float dt = ((uint32_t)now - first.t) / 60.0f;
            rate = (g_usage.h5 - first.h5) / dt;
        }
    }

    char e[32];
    if (g_usage.h5 >= 99.5f) {
        lv_line_set_points(g_ui.trProj, g_trProjPts, 0);
        fmt_eta(we, e, sizeof(e));
        snprintf(b, sizeof(b), TRS("Janela esgotada \xE2\x80\xA2 reseta em %s", "Window exhausted \xE2\x80\xA2 resets in %s"), e);
        lv_label_set_text(g_ui.trCap, b);
        lv_obj_set_style_text_color(g_ui.trCap, lv_color_hex(C_BAD), 0);
    } else if (rate > 0.02f) {
        float minsLeft = (100.0f - g_usage.h5) / rate;
        uint32_t etaT = (uint32_t)now + (uint32_t)(minsLeft * 60);
        g_trProjPts[0].x = cx;
        g_trProjPts[0].y = cy;
        if (etaT <= we) {
            g_trProjPts[1].x = tr_x(etaT, ws, we);
            g_trProjPts[1].y = tr_y(100);
            char hm[12];
            fmt_hm(etaT, hm, sizeof(hm));
            snprintf(b, sizeof(b), TRS("No ritmo atual, esgota as %s (em %dh%02dm)", "At this pace, runs out at %s (in %dh%02dm)"),
                     hm, (int)minsLeft / 60, (int)minsLeft % 60);
            lv_label_set_text(g_ui.trCap, b);
            lv_obj_set_style_text_color(g_ui.trCap, lv_color_hex(minsLeft < 60 ? C_BAD : C_WARN), 0);
        } else {
            float endPct = g_usage.h5 + rate * ((we - (uint32_t)now) / 60.0f);
            g_trProjPts[1].x = tr_x(we, ws, we);
            g_trProjPts[1].y = tr_y(endPct);
            snprintf(b, sizeof(b), TRS("No ritmo atual, NAO esgota antes do reset (~%d%%)",
                                       "At this pace, does NOT run out before reset (~%d%%)"),
                     (int)(endPct + 0.5f));
            lv_label_set_text(g_ui.trCap, b);
            lv_obj_set_style_text_color(g_ui.trCap, lv_color_hex(C_OK), 0);
        }
        lv_line_set_points(g_ui.trProj, g_trProjPts, 2);
    } else {
        lv_line_set_points(g_ui.trProj, g_trProjPts, 0);
        lv_label_set_text(g_ui.trCap, TRS("Uso estavel \xE2\x80\xA2 sem risco no momento", "Stable usage \xE2\x80\xA2 no risk right now"));
        lv_obj_set_style_text_color(g_ui.trCap, lv_color_hex(C_OK), 0);
    }
}

void heat_redraw(void)
{
    if (!g_ui.heat[0]) {
        return;
    }
    float data[24];
    heat_mode_data(g_heatMode, data);
    float mx = 1.0f;
    for (int h = 0; h < 24; h++) {
        if (data[h] > mx) {
            mx = data[h];
        }
    }
    int curHour = -1;
    time_t now = time(NULL);
    if (now > 1000000000L) {
        time_t local_t = (time_t)((long)now + (long)g_tzOffset * 3600);
        struct tm tv;
        gmtime_r(&local_t, &tv);
        curHour = tv.tm_hour;
    }
    for (int h = 0; h < 24; h++) {
        if (!g_ui.heat[h]) {
            continue;
        }
        float r = data[h] / mx;
        if (r < 0) {
            r = 0;
        }
        if (r > 1) {
            r = 1;
        }
        int hgt = 4 + (int)(r * 114);
        lv_obj_set_size(g_ui.heat[h], 13, hgt);
        lv_obj_set_y(g_ui.heat[h], 176 - hgt);
        lv_obj_set_style_bg_color(g_ui.heat[h], lv_color_hex(h == curHour ? C_TEXT : C_ACCENT), 0);
        lv_obj_set_style_bg_opa(g_ui.heat[h], (lv_opa_t)(70 + (int)(r * 185)), 0);
    }
}

void refresh_ui_values(void)
{
    if (g_state != ST_MAIN || !g_ui.agPct5) {
        return;
    }
    char b[96];

    snprintf(b, sizeof(b), "%d%%", (int)(g_usage.h5 + 0.5f));
    lv_label_set_text(g_ui.agPct5, b);
    lv_obj_set_style_text_color(g_ui.agPct5, grad_color(g_usage.h5), 0);
    set_meter(g_ui.seg5, g_usage.h5);
    snprintf(b, sizeof(b), "%d%%", (int)(g_usage.d7 + 0.5f));
    lv_label_set_text(g_ui.agPct7, b);
    lv_obj_set_style_text_color(g_ui.agPct7, grad_color(g_usage.d7), 0);
    set_meter(g_ui.seg7, g_usage.d7);

    set_chip(g_ui.agChip, overall_label(g_usage.statusOverall), status_color(g_usage.statusOverall));
    update_tok_row();

    for (int i = 0; i < NMODELS; i++) {
        if (!g_ui.mChip[i]) {
            continue;
        }
        char txt[16];
        uint32_t col;
        model_chip(i, txt, sizeof(txt), &col);
        set_chip(g_ui.mChip[i], txt, col);
        lv_obj_update_layout(g_ui.mChip[i]);
        lv_obj_set_x(g_ui.mChip[i], MODEL_CENTERS[i] - lv_obj_get_width(g_ui.mChip[i]) / 2);
    }
    if (g_ui.incident) {
        bool any = !(g_status.haikuUp && g_status.sonnetUp && g_status.opusUp && g_status.fableUp);
        lv_label_set_text(g_ui.incident,
                           !g_status.ok ? TRS("status.claude.com: sem dados", "status.claude.com: no data")
                                        : (any ? TRS("Incidente ativo \xE2\x80\xA2 veja status.claude.com",
                                                     "Active incident \xE2\x80\xA2 see status.claude.com")
                                               : TRS("status.claude.com: OK \xE2\x80\xA2 sem incidentes",
                                                     "status.claude.com: OK \xE2\x80\xA2 no incidents")));
        lv_obj_set_style_text_color(g_ui.incident, lv_color_hex(any ? C_WARN : C_FAINT), 0);
    }

    trend_redraw();
    heat_redraw();
    dash_tick();
}

void set_hdr_status(void)
{
    /* Relogio do dispositivo, ao lado do wordmark: tica a cada segundo, servindo
     * de referencia/certificacao visual de que o NTP sincronizou corretamente. */
    if (g_hdrClock) {
        char clk[12];
        fmt_now(clk, sizeof(clk));
        lv_label_set_text(g_hdrClock, clk);
    }

    if (!g_hdrStatus) {
        return;
    }
    char buf[40];
    uint32_t color;
    if (g_refreshing) {
        snprintf(buf, sizeof(buf), "%s", TRS("atualizando...", "updating..."));
        color = C_ACCENT;
    } else if (!g_lastFetchOk) {
        snprintf(buf, sizeof(buf), "%s", TRS("falha ao atualizar", "update failed"));
        color = C_BAD;
    } else {
        uint32_t s = (now_ms() - g_lastOkMs) / 1000;
        if (s < 60) {
            snprintf(buf, sizeof(buf), TRS("atualizado ha %us", "updated %us ago"), (unsigned)s);
        } else {
            snprintf(buf, sizeof(buf), TRS("atualizado ha %umin", "updated %um ago"), (unsigned)(s / 60));
        }
        color = C_MUTED;
    }
    lv_label_set_text(g_hdrStatus, buf);
    lv_obj_set_style_text_color(g_hdrStatus, lv_color_hex(color), 0);
}

static void refresh_cb(lv_event_t *e)
{
    (void)e;
    g_wantRefresh = true;
}

static void logo_spot_click_cb(lv_event_t *e)
{
    (void)e;
    static uint32_t lastClick = 0;
    static int di = 0;
    uint32_t now = now_ms();
    if (now - lastClick < 450) {
        static const int T[4] = {25, 50, 70, 100};
        show_moment((di / 4) % 2, T[di % 4]);
        di++;
        lastClick = 0;
    } else {
        lastClick = now;
    }
}

void ui_main(void)
{
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(C_BG), 0);

    lv_obj_t *hIcon = lv_image_create(scr);
    lv_image_set_src(hIcon, &img_clawd_sm);
    lv_obj_set_pos(hIcon, 14, 8);
    lv_obj_t *hWord = lv_image_create(scr);
    lv_image_set_src(hWord, &img_wordmark);
    lv_obj_set_pos(hWord, 66, 8);

    lv_obj_t *logoSpot = lv_obj_create(scr);
    lv_obj_set_pos(logoSpot, 6, 2);
    lv_obj_set_size(logoSpot, 128, 40);
    no_box(logoSpot);
    lv_obj_add_flag(logoSpot, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(logoSpot, logo_spot_click_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *ref = mkbtn(scr, LV_SYMBOL_REFRESH, &lv_font_montserrat_20, C_SURFACE2, C_ACCENT);
    lv_obj_set_size(ref, 56, 40);
    lv_obj_set_ext_click_area(ref, 10);
    lv_obj_align(ref, LV_ALIGN_TOP_MID, 0, 2);
    lv_obj_add_event_cb(ref, refresh_cb, LV_EVENT_CLICKED, NULL);

    g_hdrClock = mklabel(scr, "--:--:--", &lv_font_montserrat_12, C_MUTED);
    lv_obj_set_pos(g_hdrClock, 132, 18);

    g_hdrStatus = mklabel(scr, "", &lv_font_montserrat_12, C_MUTED);
    lv_obj_align(g_hdrStatus, LV_ALIGN_TOP_RIGHT, -92, 16);

    lv_obj_t *gear = mkbtn(scr, LV_SYMBOL_SETTINGS, &lv_font_montserrat_22, C_SURFACE2, C_TEXT);
    lv_obj_set_size(gear, 78, 40);
    lv_obj_set_ext_click_area(gear, 16);
    lv_obj_align(gear, LV_ALIGN_TOP_RIGHT, -6, 2);
    lv_obj_add_event_cb(gear, nav_cb, LV_EVENT_CLICKED, (void *)(intptr_t)ST_SETTINGS);

    g_ui.refBar = lv_bar_create(scr);
    lv_obj_set_size(g_ui.refBar, 480, 3);
    lv_obj_set_pos(g_ui.refBar, 0, 40);
    lv_bar_set_range(g_ui.refBar, 0, 1000);
    lv_bar_set_value(g_ui.refBar, 1000, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(g_ui.refBar, lv_color_hex(C_SURFACE), LV_PART_MAIN);
    lv_obj_set_style_bg_color(g_ui.refBar, lv_color_hex(C_ACCENT), LV_PART_INDICATOR);
    lv_obj_set_style_radius(g_ui.refBar, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(g_ui.refBar, 0, LV_PART_INDICATOR);
    lv_obj_clear_flag(g_ui.refBar, LV_OBJ_FLAG_CLICKABLE);

    g_ui.tv = lv_tileview_create(scr);
    lv_obj_set_pos(g_ui.tv, 0, 46);
    lv_obj_set_size(g_ui.tv, 480, 250);
    lv_obj_set_style_bg_opa(g_ui.tv, 0, 0);
    lv_obj_set_style_border_width(g_ui.tv, 0, 0);
    lv_obj_set_scrollbar_mode(g_ui.tv, LV_SCROLLBAR_MODE_OFF);
    for (int i = 0; i < NTILES; i++) {
        g_ui.tile[i] = lv_tileview_add_tile(g_ui.tv, i, 0, LV_DIR_HOR);
        tile_setup(g_ui.tile[i]);
    }
    build_tile_agora(g_ui.tile[0]);
    build_tile_models(g_ui.tile[1]);
    build_tile_trend(g_ui.tile[2]);
    build_tile_heat(g_ui.tile[3]);
    lv_obj_add_event_cb(g_ui.tv, on_tile_changed, LV_EVENT_VALUE_CHANGED, NULL);

    for (int i = 0; i < NTILES; i++) {
        g_ui.dots[i] = lv_obj_create(scr);
        lv_obj_set_size(g_ui.dots[i], 8, 8);
        lv_obj_set_style_radius(g_ui.dots[i], 4, 0);
        lv_obj_set_style_bg_color(g_ui.dots[i], lv_color_hex(C_BORDER), 0);
        lv_obj_set_style_border_width(g_ui.dots[i], 0, 0);
        lv_obj_clear_flag(g_ui.dots[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_align(g_ui.dots[i], LV_ALIGN_BOTTOM_MID, (int)((i - (NTILES - 1) / 2.0f) * 18), -4);
    }

    refresh_ui_values();
    on_tile_changed(NULL);
}

/* ============================================================
 * Animacao/ticks do dashboard (chamado a cada iteracao do loop principal
 * enquanto g_state==ST_MAIN) — porte do bloco "if (g_state == ST_MAIN)" de
 * loop() no claude_stick.ino.
 * ============================================================ */
void dashboard_animate(uint32_t now)
{
    static uint32_t lastTick = 0, lastBar = 0, lastBob = 0, blinkAt = 0;
    static bool blinkClosed = false;

    if (now - lastTick > 1000) {
        lastTick = now;
        dash_tick();
        update_tok_row();
    }
    if (now - lastBar > 250 && g_ui.refBar) {
        lastBar = now;
        int v;
        if (g_refreshing) {
            v = 1000;
        } else {
            uint32_t el = now - g_lastPollMs, per = (uint32_t)g_pollSec * 1000;
            v = el >= per ? 0 : (int)(1000 - (uint64_t)el * 1000 / per);
        }
        lv_bar_set_value(g_ui.refBar, v, LV_ANIM_OFF);
    }
    if (now - lastBob > 80) {
        lastBob = now;
        float ph = now / 600.0f;
        for (int i = 0; i < g_mascN; i++) {
            if (!g_masc[i].cont) {
                continue;
            }
            if (g_masc[i].mood == 1) {
                lv_obj_set_y(g_masc[i].cont, g_masc[i].baseY + (int)(2.0f * sinf(ph + i * 0.9f) - 1.0f));
            } else if (g_masc[i].mood == 2) {
                lv_obj_set_y(g_masc[i].cont, g_masc[i].baseY + (int)(1.2f * sinf(ph * 0.6f + i)));
                if (g_masc[i].drop) {
                    uint32_t cyc = (now + i * 300) % 900;
                    lv_obj_set_y(g_masc[i].drop, 24 + (int)(cyc * 22 / 900));
                    lv_obj_set_style_bg_opa(g_masc[i].drop, (lv_opa_t)(255 - cyc * 190 / 900), 0);
                }
            }
        }
    }
    uint32_t bp = blinkClosed ? 150 : 3000;
    if (now - blinkAt > bp) {
        blinkAt = now;
        blinkClosed = !blinkClosed;
        for (int i = 0; i < g_mascN; i++) {
            if (g_masc[i].mood != 1) {
                continue;
            }
            for (int k = 0; k < 2; k++) {
                if (!g_masc[i].lid[k]) {
                    continue;
                }
                if (blinkClosed) {
                    lv_obj_clear_flag(g_masc[i].lid[k], LV_OBJ_FLAG_HIDDEN);
                } else {
                    lv_obj_add_flag(g_masc[i].lid[k], LV_OBJ_FLAG_HIDDEN);
                }
            }
        }
    }
    if (g_slideSec > 0 && g_ui.tv && !g_refreshing && !g_mo.scrim && now - g_lastTouchMs > 10000 &&
        now - g_lastSlideMs > (uint32_t)g_slideSec * 1000) {
        g_lastSlideMs = now;
        int next = (g_curTile + 1) % NTILES;
        lv_tileview_set_tile_by_index(g_ui.tv, next, 0, LV_ANIM_ON);
    }

    if (g_pendWin >= 0 && !g_mo.scrim && !g_refreshing) {
        show_moment(g_pendWin, g_pendThr);
        g_pendWin = -1;
    }
    if (g_mo.scrim) {
        moment_tick();
    }
}
