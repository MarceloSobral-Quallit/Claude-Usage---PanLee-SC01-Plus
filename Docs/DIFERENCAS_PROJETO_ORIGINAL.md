# Diferenças entre o projeto original e este porte

Este documento é a referência **completa** de tudo que mudou entre o
**Claude Usage Stick** original (Arduino, ESP32-S3, display QSPI AXS15231B) e este
firmware, portado para a placa **Smart Panlee SC01 Plus** em **ESP-IDF + LVGL 9**.
O material de referência do projeto original não faz parte deste repositório
(ficou só como insumo local de desenvolvimento). O `README.md` da raiz traz o
resumo curto (seção "O que veio do projeto original"); aqui está o detalhe
completo.

Este projeto também bebeu do **pacote inicial de bring-up da placa**
(`SC01_PLUS_STARTER`) — a Parte 2 deste documento cobre essa segunda origem
separadamente, já que ela contribuiu só a camada de hardware (display/touch/SD),
não a UI/produto.

---

## Parte 1 — Claude Usage Stick original → este porte

### O que foi reaproveitado quase literalmente

| Item | Como |
|---|---|
| UI em LVGL 9 (telas, mascotes, medidores, gráfico, heatmap, animações de limiar) | A UI original já usava API exclusiva do LVGL 9 (`lv_display_create`, `lv_image_create`, `lv_buttonmatrix`), então o código de construção de tela é praticamente o mesmo — só a camada de driver embaixo mudou. |
| Leitura do rate-limit pelos headers `anthropic-ratelimit-unified-*` | Mesma lógica de parsing, adaptada de `HTTPClient.collectHeaders()` (Arduino) para o event handler `HTTP_EVENT_ON_HEADER` do `esp_http_client`. |
| Sonda de saúde por modelo + cruzamento com `status.claude.com` | Mesmo algoritmo (1 modelo por ciclo, revezando; humor do mascote por código HTTP). |
| Criptografia do token (AES-256-GCM, chave derivada do PIN via SHA-256/10k rounds) | mbedTLS é a mesma API em Arduino-ESP32 e ESP-IDF — porte quase byte a byte. |
| Conceito de produto, paleta de cores, mascotes Clawd | Sem alteração. |

### O que foi reescrito

| Item | Original (Arduino/QSPI) | Este porte (ESP-IDF/i80) |
|---|---|---|
| Display | AXS15231B, barramento QSPI, `Arduino_GFX` | ST7796UI, barramento i80/8080, `esp_lcd` + componente `esp_lcd_st7796` |
| Buffer do LVGL | Full-frame (480×320×2 ≈ 300 KB) em **PSRAM OPI** (`LV_DISPLAY_RENDER_MODE_FULL`) | Parcial (20 linhas) em **RAM interna** (`LV_DISPLAY_RENDER_MODE_PARTIAL`) — o painel i80 tem GRAM própria, não precisa de framebuffer completo |
| Touch | AXS15231B capacitivo, I²C `0x3B`, biblioteca própria | FT6336U, I²C `0x38`, driver manual (`app_main.c`) |
| Rede/TLS | `WiFiClientSecure` + `HTTPClient`, CA bundle customizado embutido (`certs.cpp`) | `esp_http_client` + bundle de CAs nativo do mbedTLS (`esp_crt_bundle_attach`) — não precisa de certificado customizado no repo |
| Onboarding Wi-Fi | Classe `WiFiManager` (Arduino), `Preferences` para até 3 redes | Módulo `wifi_manager.c` sobre `esp_wifi` + NVS direto, mesma semântica (até 3 redes, promoção do mais recente) |
| Servidor web + mDNS | `WebServer` + `ESPmDNS`, uma instância criada/destruída por tela (`start_data_web()`/`stop_web()`) | `esp_http_server` + componente `mdns`, **uma única instância persistente** desde o boot (mais simples, sempre responde a `/window`/`/tokens` mesmo fora do dashboard) |
| Histórico/heatmap | Arquivo binário em **LittleFS interno** (`/hist.bin`) | Arquivo binário no **microSD** (`HISTORY.DAT`) — decisão do usuário, reaproveita o slot já validado nesta placa; histórico fica vazio se o cartão for removido |
| Busca de rede (fetch periódico) | **Bloqueia** a task única do Arduino (`loop()`) por ~1-2s a cada ciclo | Roda numa **task FreeRTOS dedicada** (`refresh_task_fn`); a tela continua responsiva durante o fetch |
| Backlight | PWM via `ledcAttach`/`ledcWrite` (Arduino) | PWM via driver `ledc` nativo do ESP-IDF (`bsp_display_set_brightness`) |
| NTP/fuso horário | `configTime()` (Arduino) ajusta TZ do processo direto | SNTP puro (`esp_netif_sntp`) mantém o sistema em **UTC**; o fuso é somado manualmente só na hora de formatar (ver `REFERENCIA-HARDWARE-LVGL.md` § armadilhas) |

### Funcionalidades novas nesta versão (não existem no original)

- **Ajustes → "Solicitar PIN no boot: Sim/Não"** — permite desligar a exigência de
  digitar o PIN a cada boot (usa um PIN fixo interno automaticamente). Troca de
  segurança explícita, documentada na tela e no README § Segurança.
- **Validação do prefixo do token antes da rede**: se o texto colado não começar
  com `sk-ant-oat01-`, o firmware recusa na hora (sem gastar uma chamada HTTPS) e
  explica o motivo — evita o erro comum de colar uma chave de API comum
  (`sk-ant-api03-...`) em vez do token OAuth do `claude setup-token`.
- **Ambiente de build local ao repositório** (`.idf-env/`, `tools/setup_esp_idf_env.ps1`)
  — o original usa `arduino-cli` com libs globais; aqui nada é instalado fora do repo.

### Mapa de arquivos — original → porte

| Original (`claude-usage-stick-SVGL-main/firmware/claude_stick/`) | Porte (`firmware/panlee_sc01_plus/main/`) |
|---|---|
| `claude_stick.ino` (2270 linhas, tudo num arquivo) | Dividido em `app_main.c`, `app_state.c/h`, `ui_common.c`, `ui_onboarding.c`, `ui_dashboard.c`, `ui_settings.c`, `ui_moments.c` |
| `config.h` | `config.h` + `board.h` (pinagem separada) |
| `api.cpp`/`api.h` | `api.c`/`api.h` |
| `status.cpp`/`status.h` | `status.c`/`status.h` |
| `crypto.cpp`/`crypto.h` | `crypto.c`/`crypto.h` |
| `certs.cpp`/`certs.h` | *(eliminado — usa `esp_crt_bundle_attach`)* |
| `wifi_manager.h` (classe C++) | `wifi_manager.c`/`wifi_manager.h` (módulo C + NVS) |
| `touch.h` | Driver de touch embutido em `app_main.c` |
| `logo_assets.h` | `logo_assets.h` (copiado sem alteração — já é formato LVGL 9) |
| `lv_conf.h` | `lv_conf.h` (mesmo conteúdo, adaptado para o componente gerenciado do ESP-IDF) |
| *(sem equivalente — WebServer instanciado por tela)* | `onboarding_server.c`/`onboarding_server.h` (servidor único persistente) |
| *(sem equivalente — LittleFS direto no `.ino`)* | `storage.c`/`storage.h` (NVS + arquivo no SD) |
| *(sem equivalente — `configTime()` inline)* | `time_sync.c`/`time_sync.h` |

---

## Parte 2 — Pacote inicial da placa (`SC01_PLUS_STARTER`) → este porte

O pacote inicial contribuiu só a camada de **bring-up de hardware**, validada
fisicamente antes de qualquer código de produto entrar:

- Reaproveitado: pinagem, contrato de cor do LCD (BGR + byte swap + inversão +
  espelhamento), driver base do ST7796 (`bsp_display.c`), montagem do microSD
  (`bsp_sdcard.c`), log persistente (`bsp_debug_log.c`).
- **Removido** (recursos de bring-up sem relação com o produto, por decisão
  explícita ao iniciar o porte): mouse BLE HID, slideshow de JPEG via SD,
  servidor de arquivos genérico (`/files`, `/file`, `/upload`).
- **Trocado**: Wi-Fi por credencial fixa em `wifi_credentials.h` (gerado do
  `wifi_cred.txt` via CMake, obrigatório) → onboarding 100% pela tela, com
  `wifi_cred.txt` virando **opcional** (só pré-semeia uma rede de conveniência
  para desenvolvimento).
- **Trocado**: LVGL 8.3.11 (validado pelo pacote inicial) → LVGL 9.2.2 (ver
  `REFERENCIA-HARDWARE-LVGL.md` para a justificativa completa).

---

## Bugs encontrados e corrigidos só nesta placa (não existiam/não se aplicam ao original)

Detalhados em `firmware/panlee_sc01_plus/REFERENCIA-HARDWARE-LVGL.md` §
Armadilhas e em `docs/REGISTRO_DE_BUILDS.md`:

1. `CONFIG_LV_CONF_SKIP` do componente `lvgl/lvgl` ignorando `main/lv_conf.h`.
2. Include path do componente `lvgl/lvgl` não alcançando `main/lv_conf.h` (corrigido com `LV_CONF_PATH` global).
3. Stack overflow na task `"main"` (display+Wi-Fi+mDNS+HTTP+SD tudo inline).
4. Stack overflow na task do `esp_http_server` (handler de `/token` fazendo uma chamada HTTPS completa).
5. SNTP nunca inicializava (`CONFIG_LWIP_SNTP_MAX_SERVERS` menor que o número de servidores configurados).
6. Power-save do Wi-Fi corrompendo handshakes TLS consecutivos (corrigido com `esp_wifi_set_ps(WIFI_PS_NONE)`).

Nenhum desses bugs existe no firmware original porque ele roda noutro
hardware/toolchain (Arduino, sem tasks FreeRTOS explícitas do jeito que este
projeto usa, sem esse componente LVGL gerenciado).
