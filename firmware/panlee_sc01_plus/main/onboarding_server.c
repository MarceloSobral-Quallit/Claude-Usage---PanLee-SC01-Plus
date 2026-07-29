#include "onboarding_server.h"
#include "config.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "mdns.h"

static const char *TAG = "ONBOARD_SRV";
static httpd_handle_t s_server;
static onboarding_server_callbacks_t s_cb;
static bool s_mdns_started;

static const char TOKEN_FORM_HTML[] =
    "<!doctype html><html lang=\"pt-BR\"><head><meta charset=\"utf-8\">"
    "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
    "<title>Claude Usage Stick — configurar token</title>"
    "<style>body{font-family:sans-serif;max-width:480px;margin:2rem auto;padding:0 1rem}"
    "input{width:100%;padding:.6rem;font-size:1rem;margin:.5rem 0}"
    "button{padding:.6rem 1.2rem;font-size:1rem}"
    ".alert{background:#fff3cd;border:1px solid #ffe69c;border-radius:8px;padding:.8rem 1rem;"
    "margin:1rem 0;font-size:.92rem}"
    ".alert code{background:#00000014;padding:.1rem .3rem;border-radius:4px}"
    "</style></head><body>"
    "<h1>Claude Usage Stick</h1>"
    "<div class=\"alert\">"
    "<b>&#9888; Atencao ao tipo de token:</b> este campo aceita <b>somente</b> o token "
    "OAuth gerado pelo comando <code>claude setup-token</code> (Claude Code CLI), que "
    "comeca com <code>sk-ant-oat01-</code>. Uma chave de API comum "
    "(<code>sk-ant-api03-...</code>, do console da Anthropic) <b>nao funciona aqui</b> "
    "e sera recusada."
    "</div>"
    "<form method=\"POST\" action=\"/token\">"
    "<input type=\"text\" name=\"token\" placeholder=\"sk-ant-oat01-...\" autocomplete=\"off\">"
    "<button type=\"submit\">Salvar token</button>"
    "</form></body></html>";

static esp_err_t root_handler(httpd_req_t *req)
{
    if (s_cb.is_token_configured != NULL && s_cb.is_token_configured()) {
        char body[128];
        int len = snprintf(body, sizeof(body), "claude-usage-stick\nfirmware: %s\ntoken: configurado\n", FW_VERSION);
        httpd_resp_set_type(req, "text/plain; charset=utf-8");
        return httpd_resp_send(req, body, len);
    }
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, TOKEN_FORM_HTML, HTTPD_RESP_USE_STRLEN);
}

/* Decodifica minimamente application/x-www-form-urlencoded (%XX e '+'). */
static void url_decode(char *s)
{
    char *out = s;
    while (*s != '\0') {
        if (*s == '+') {
            *out++ = ' ';
            s++;
        } else if (*s == '%' && isxdigit((unsigned char)s[1]) && isxdigit((unsigned char)s[2])) {
            char hex[3] = {s[1], s[2], '\0'};
            *out++ = (char)strtol(hex, NULL, 16);
            s += 3;
        } else {
            *out++ = *s++;
        }
    }
    *out = '\0';
}

static esp_err_t token_post_handler(httpd_req_t *req)
{
    char buf[320];
    int total = 0;
    int remaining = req->content_len;
    if (remaining <= 0 || remaining >= (int)sizeof(buf)) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "corpo invalido");
    }
    while (remaining > 0) {
        int r = httpd_req_recv(req, buf + total, remaining);
        if (r <= 0) {
            return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "falha ao receber");
        }
        total += r;
        remaining -= r;
    }
    buf[total] = '\0';

    char token[300] = "";
    char param[300];
    if (httpd_query_key_value(buf, "token", param, sizeof(param)) == ESP_OK) {
        snprintf(token, sizeof(token), "%s", param);
        url_decode(token);
    }

    bool accepted = token[0] != '\0' && s_cb.on_token_submitted != NULL && s_cb.on_token_submitted(token);
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    if (accepted) {
        return httpd_resp_sendstr(req, "<html><body><h1>Token salvo</h1>"
                                        "<p>Volte para o dispositivo e defina o PIN.</p></body></html>");
    }
    httpd_resp_set_status(req, "400 Bad Request");
    char body[384];
    const char *reason = s_cb.get_token_error != NULL ? s_cb.get_token_error() : "";
    snprintf(body, sizeof(body),
             "<html><body><h1>Token invalido</h1>"
             "<p>Motivo: %s</p>"
             "<p>Confira se o token comeca com <code>sk-ant-oat01-</code> e foi gerado com "
             "<code>claude setup-token</code> (nao use uma chave de API comum "
             "<code>sk-ant-api03-...</code>). <a href=\"/\">Voltar e tentar de novo</a>.</p>"
             "</body></html>",
             reason[0] ? reason : "desconhecido");
    return httpd_resp_sendstr(req, body);
}

static esp_err_t window_get_handler(httpd_req_t *req)
{
    uint32_t reset_epoch = s_cb.get_h5_reset_epoch != NULL ? s_cb.get_h5_reset_epoch() : 0;
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "h5_reset", reset_epoch);
    char *json = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    esp_err_t err = httpd_resp_sendstr(req, json);
    cJSON_free(json);
    cJSON_Delete(root);
    return err;
}

static esp_err_t tokens_post_handler(httpd_req_t *req)
{
    char buf[512];
    int total = 0;
    int remaining = req->content_len;
    if (remaining <= 0 || remaining >= (int)sizeof(buf)) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "corpo invalido");
    }
    while (remaining > 0) {
        int r = httpd_req_recv(req, buf + total, remaining);
        if (r <= 0) {
            return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "falha ao receber");
        }
        total += r;
        remaining -= r;
    }
    buf[total] = '\0';

    cJSON *root = cJSON_Parse(buf);
    if (root == NULL) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "JSON invalido");
    }
    long long tin = (long long)cJSON_GetNumberValue(cJSON_GetObjectItem(root, "in"));
    long long tout = (long long)cJSON_GetNumberValue(cJSON_GetObjectItem(root, "out"));
    long long cache = (long long)cJSON_GetNumberValue(cJSON_GetObjectItem(root, "cache"));
    int sessions = (int)cJSON_GetNumberValue(cJSON_GetObjectItem(root, "sessions"));
    cJSON_Delete(root);

    if (s_cb.on_tokens_pushed != NULL) {
        s_cb.on_tokens_pushed(tin, tout, cache, sessions);
    }
    return httpd_resp_sendstr(req, "ok\n");
}

static void start_mdns(void)
{
    if (s_mdns_started) {
        return;
    }
    if (mdns_init() != ESP_OK) {
        ESP_LOGW(TAG, "mdns_init falhou");
        return;
    }
    mdns_hostname_set(ONBOARDING_HOSTNAME);
    mdns_instance_name_set("Claude Usage Stick");
    mdns_service_add(NULL, "_http", "_tcp", ONBOARDING_HTTP_PORT, NULL, 0);
    s_mdns_started = true;
    ESP_LOGI(TAG, "mDNS: %s.local", ONBOARDING_HOSTNAME);
}

void onboarding_server_start(const onboarding_server_callbacks_t *callbacks)
{
    if (s_server != NULL) {
        return;
    }
    s_cb = *callbacks;
    start_mdns();

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 4;
    /* O handler de POST /token chama fetchUsage() (uma requisicao HTTPS/TLS
     * completa contra a API da Anthropic) direto na task do httpd. O stack
     * default do esp_http_server (4096 bytes) estoura nesse caminho
     * (confirmado em hardware: reset com "Saved PC: _WindowOverflow8",
     * assinatura classica de stack overflow no Xtensa) — mesma classe de
     * bug ja corrigida para a task "main" via CONFIG_ESP_MAIN_TASK_STACK_SIZE. */
    config.stack_size = 8192;
    if (httpd_start(&s_server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "httpd_start falhou");
        s_server = NULL;
        return;
    }

    const httpd_uri_t root_uri = {.uri = "/", .method = HTTP_GET, .handler = root_handler};
    const httpd_uri_t token_uri = {.uri = "/token", .method = HTTP_POST, .handler = token_post_handler};
    const httpd_uri_t window_uri = {.uri = "/window", .method = HTTP_GET, .handler = window_get_handler};
    const httpd_uri_t tokens_uri = {.uri = "/tokens", .method = HTTP_POST, .handler = tokens_post_handler};
    httpd_register_uri_handler(s_server, &root_uri);
    httpd_register_uri_handler(s_server, &token_uri);
    httpd_register_uri_handler(s_server, &window_uri);
    httpd_register_uri_handler(s_server, &tokens_uri);
    ESP_LOGI(TAG, "servidor de onboarding iniciado na porta %d", ONBOARDING_HTTP_PORT);
}

void onboarding_server_stop(void)
{
    if (s_server != NULL) {
        httpd_stop(s_server);
        s_server = NULL;
    }
}
