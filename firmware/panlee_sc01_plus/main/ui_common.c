#include "app_state.h"
#include "logo_assets.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

/* ============================================================
 * Helpers de UI (porte de claude_stick.ino)
 * ============================================================ */

lv_obj_t *mklabel(lv_obj_t *p, const char *txt, const lv_font_t *font, uint32_t color)
{
    lv_obj_t *l = lv_label_create(p);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    return l;
}

void no_box(lv_obj_t *o)
{
    lv_obj_set_style_bg_opa(o, 0, 0);
    lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_set_style_pad_all(o, 0, 0);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
}

lv_obj_t *mkbtn(lv_obj_t *p, const char *txt, const lv_font_t *font, uint32_t bg, uint32_t fg)
{
    lv_obj_t *b = lv_button_create(p);
    lv_obj_set_style_bg_color(b, lv_color_hex(bg), 0);
    lv_obj_set_style_radius(b, 10, 0);
    lv_obj_set_style_shadow_width(b, 0, 0);
    lv_obj_center(mklabel(b, txt, font, fg));
    return b;
}

uint32_t pct_color(float p)
{
    if (p < 70.0f) {
        return C_OK;
    }
    if (p < 90.0f) {
        return C_WARN;
    }
    return C_BAD;
}

lv_color_t grad_color(float p)
{
    if (p < 0) {
        p = 0;
    }
    if (p > 100) {
        p = 100;
    }
    if (p <= 50.0f) {
        return lv_color_mix(lv_color_hex(C_WARN), lv_color_hex(C_OK), (uint8_t)(p * 255.0f / 50.0f));
    }
    return lv_color_mix(lv_color_hex(C_BAD), lv_color_hex(C_WARN), (uint8_t)((p - 50.0f) * 255.0f / 50.0f));
}

void set_meter(lv_obj_t **seg, float pct)
{
    int filled = (int)(pct / 100.0f * NSEG + 0.5f);
    if (pct > 0.5f && filled == 0) {
        filled = 1;
    }
    if (filled > NSEG) {
        filled = NSEG;
    }
    lv_color_t col = grad_color(pct);
    for (int i = 0; i < NSEG; i++) {
        if (!seg[i]) {
            continue;
        }
        lv_obj_set_style_bg_color(seg[i], (i < filled) ? col : lv_color_hex(C_TRACK), 0);
        lv_obj_set_style_bg_opa(seg[i], (i < filled) ? LV_OPA_COVER : 160, 0);
    }
}

void fmt_eta(uint32_t epoch, char *out, int sz)
{
    time_t now = time(NULL);
    if (now < 1000000000L || epoch == 0) {
        snprintf(out, sz, "--");
        return;
    }
    long d = (long)epoch - (long)now;
    if (d <= 0) {
        snprintf(out, sz, "%s", TRS("agora", "now"));
        return;
    }
    int days = d / 86400;
    d %= 86400;
    int hrs = d / 3600;
    d %= 3600;
    int mins = d / 60;
    if (days > 0) {
        snprintf(out, sz, "%dd %dh", days, hrs);
    } else if (hrs > 0) {
        snprintf(out, sz, "%dh %02dm", hrs, mins);
    } else {
        snprintf(out, sz, "%dm", mins);
    }
}

/* Hora local = UTC + g_tzOffset*3600, extraida via gmtime_r (o sistema fica
 * sempre em UTC; nao ha TZ de processo configurado, ver app_state.c). */
static void local_broken_down_time(time_t utc, struct tm *out)
{
    time_t local_t = (time_t)((long)utc + (long)g_tzOffset * 3600);
    gmtime_r(&local_t, out);
}

void fmt_clock(uint32_t epoch, char *out, int sz)
{
    if (epoch == 0 || time(NULL) < 1000000000L) {
        strlcpy(out, "--:--", sz);
        return;
    }
    struct tm tmv;
    local_broken_down_time((time_t)epoch, &tmv);
    strftime(out, sz, "%a %H:%M", &tmv);
}

void fmt_hm(uint32_t epoch, char *out, int sz)
{
    if (epoch == 0 || time(NULL) < 1000000000L) {
        strlcpy(out, "--:--", sz);
        return;
    }
    struct tm tmv;
    local_broken_down_time((time_t)epoch, &tmv);
    strftime(out, sz, "%H:%M", &tmv);
}

void fmt_now(char *out, int sz)
{
    time_t now = time(NULL);
    if (now < 1000000000L) {
        strlcpy(out, "--:--:--", sz);
        return;
    }
    struct tm tmv;
    local_broken_down_time(now, &tmv);
    strftime(out, sz, "%H:%M:%S", &tmv);
}

void fmt_tok(long long v, char *out, int sz)
{
    if (v >= 100000000LL) {
        snprintf(out, sz, "%lldM", v / 1000000LL);
    } else if (v >= 1000000LL) {
        snprintf(out, sz, "%.1fM", v / 1e6);
    } else if (v >= 10000LL) {
        snprintf(out, sz, "%lldk", v / 1000LL);
    } else if (v >= 1000LL) {
        snprintf(out, sz, "%.1fk", v / 1e3);
    } else {
        snprintf(out, sz, "%lld", v);
    }
}

static void anim_opa_cb(void *o, int32_t v)
{
    lv_obj_set_style_opa((lv_obj_t *)o, (lv_opa_t)v, 0);
}

lv_obj_t *build_claude_mark(lv_obj_t *parent)
{
    lv_obj_t *img = lv_image_create(parent);
    lv_image_set_src(img, &img_clawd_big);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, img);
    lv_anim_set_exec_cb(&a, anim_opa_cb);
    lv_anim_set_values(&a, 140, 255);
    lv_anim_set_duration(&a, 900);
    lv_anim_set_playback_duration(&a, 900);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);
    return img;
}

lv_obj_t *tlabel(lv_obj_t *p, const lv_font_t *f, uint32_t c, int x, int y)
{
    lv_obj_t *l = lv_label_create(p);
    lv_obj_set_style_text_font(l, f, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(c), 0);
    lv_label_set_text(l, "");
    lv_obj_set_pos(l, x, y);
    return l;
}

lv_obj_t *tstatic(lv_obj_t *p, const char *txt, const lv_font_t *f, uint32_t c, int x, int y)
{
    lv_obj_t *l = mklabel(p, txt, f, c);
    lv_obj_set_pos(l, x, y);
    return l;
}

void tile_setup(lv_obj_t *t)
{
    lv_obj_set_style_bg_opa(t, 0, 0);
    lv_obj_set_style_border_width(t, 0, 0);
    lv_obj_set_style_pad_all(t, 0, 0);
    lv_obj_clear_flag(t, LV_OBJ_FLAG_SCROLLABLE);
}

lv_obj_t *card(lv_obj_t *p, int x, int y, int w, int h)
{
    lv_obj_t *c = lv_obj_create(p);
    lv_obj_set_pos(c, x, y);
    lv_obj_set_size(c, w, h);
    lv_obj_set_style_bg_color(c, lv_color_hex(C_SURFACE), 0);
    lv_obj_set_style_border_width(c, 0, 0);
    lv_obj_set_style_radius(c, 18, 0);
    lv_obj_set_style_pad_all(c, 14, 0);
    lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);
    return c;
}

lv_obj_t *mkchip(lv_obj_t *p, int x, int y)
{
    lv_obj_t *o = lv_obj_create(p);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, LV_SIZE_CONTENT, 24);
    lv_obj_set_style_radius(o, 12, 0);
    lv_obj_set_style_pad_hor(o, 10, 0);
    lv_obj_set_style_pad_ver(o, 0, 0);
    lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *l = lv_label_create(o);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_12, 0);
    lv_label_set_text(l, "");
    lv_obj_center(l);
    return o;
}

void set_chip(lv_obj_t *o, const char *txt, uint32_t col)
{
    if (!o) {
        return;
    }
    lv_color_t c = lv_color_hex(col);
    lv_obj_set_style_bg_color(o, lv_color_mix(c, lv_color_hex(C_BG), 60), 0);
    lv_obj_t *l = lv_obj_get_child(o, 0);
    if (l) {
        lv_label_set_text(l, txt[0] ? txt : "--");
        lv_obj_set_style_text_color(l, c, 0);
    }
}

lv_obj_t *rrect(lv_obj_t *p, int x, int y, int w, int h, int r, uint32_t col)
{
    lv_obj_t *o = lv_obj_create(p);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_radius(o, r, 0);
    lv_obj_set_style_bg_color(o, lv_color_hex(col), 0);
    lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_set_style_pad_all(o, 0, 0);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    return o;
}

uint32_t status_color(const char *s)
{
    if (!s || !s[0]) {
        return C_MUTED;
    }
    if (!strcmp(s, "rejected") || !strcmp(s, "rate_limited") || !strcmp(s, "exceeded")) {
        return C_BAD;
    }
    if (strstr(s, "warning")) {
        return C_WARN;
    }
    return C_OK;
}

const char *overall_label(const char *s)
{
    if (!s || !s[0]) {
        return "--";
    }
    if (!strcmp(s, "allowed")) {
        return "OK";
    }
    if (strstr(s, "warning")) {
        return TRS("ATENCAO", "WARNING");
    }
    if (!strcmp(s, "rejected")) {
        return TRS("BLOQUEADO", "BLOCKED");
    }
    return s;
}

void ui_message(const char *title, const char *sub, uint32_t color)
{
    lv_obj_t *scr = lv_screen_active();
    lv_obj_t *t = mklabel(scr, title, &lv_font_montserrat_24, color);
    lv_obj_align(t, LV_ALIGN_CENTER, 0, -16);
    if (sub && sub[0]) {
        lv_obj_t *s = mklabel(scr, sub, &lv_font_montserrat_16, C_MUTED);
        lv_obj_align(s, LV_ALIGN_CENTER, 0, 20);
    }
}

void ui_loading(const char *sub)
{
    lv_obj_t *scr = lv_screen_active();
    lv_obj_t *mark = build_claude_mark(scr);
    lv_obj_align(mark, LV_ALIGN_CENTER, 0, -50);
    lv_obj_t *t = mklabel(scr, TRS("Carregando seu uso...", "Loading your usage..."), &lv_font_montserrat_18, C_TEXT);
    lv_obj_align(t, LV_ALIGN_CENTER, 0, 28);
    if (sub && sub[0]) {
        lv_obj_t *s = mklabel(scr, sub, &lv_font_montserrat_12, C_MUTED);
        lv_obj_align(s, LV_ALIGN_CENTER, 0, 52);
    }
    lv_obj_t *spn = lv_spinner_create(scr);
    lv_spinner_set_anim_params(spn, 1200, 70);
    lv_obj_set_size(spn, 34, 34);
    lv_obj_align(spn, LV_ALIGN_CENTER, 0, 90);
    lv_obj_set_style_arc_color(spn, lv_color_hex(C_SURFACE2), LV_PART_MAIN);
    lv_obj_set_style_arc_color(spn, lv_color_hex(C_ACCENT), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(spn, 4, LV_PART_MAIN);
    lv_obj_set_style_arc_width(spn, 4, LV_PART_INDICATOR);
}
