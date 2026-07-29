# Escopo funcional e estado atual — Claude Usage Stick (Panlee SC01 Plus)

> Este documento descreve **o que está entregue e em que estado de validação**, para
> orientar o roadmap em `DEV_PLAYBOOK.md`. Para a descrição de produto (o que o
> dispositivo faz, telas, screenshots conceituais), ver o [`README.md`](../README.md)
> da raiz. Atualizado em 2026-07-29.

## O que é

Porte completo do firmware **Claude Usage Stick** (originalmente Arduino/ESP32-S3 com
display QSPI AXS15231B) para a placa **Smart Panlee / Wireless-Tag SC01 Plus**
(ESP32-S3, LCD ST7796UI via barramento i80, touch FT6336U), reescrito em **ESP-IDF
puro + LVGL 9.2** (`firmware/panlee_sc01_plus/`).

## Estado atual (validado em hardware real, COM3)

| Área | Estado | Evidência |
|---|---|---|
| Build (ESP-IDF local ao repo) | ✅ Compila do zero (`idf.py build`) | `docs/REGISTRO_DE_BUILDS.md` |
| LCD (ST7796/i80, paisagem 480×320) | ✅ Inicializa, contrato de cor correto | log serial: `LCD initialized at 10 MHz, logical 480x320` |
| Touch (FT6336U) | ⚠️ Inicializado; toque interativo ainda não confirmado pelo usuário na tela | pendente |
| Wi-Fi (onboarding + auto-connect) | ✅ Conecta com rede pré-semeada (`wifi_cred.txt`) | log serial: `WIFI_MGR: connected; IP=...` |
| microSD (montagem, histórico) | ✅ Monta, lista arquivos; `HISTORY.DAT` ainda não gravado (sem fetch bem-sucedido ainda) | log serial |
| mDNS + servidor de onboarding | ✅ `claude-stick.local`, porta 80 ativa | log serial |
| Onboarding — tela de token (UI) | ✅ Mostra IP, aguarda POST /token | confirmado visualmente |
| Onboarding — validação do token (API) | ❌ **Bloqueado**: API da Anthropic rejeita com erro classe 401 | ver `DEV_PLAYBOOK.md` § pendências |
| PIN / dashboard / settings / moments | ⏳ Não alcançável ainda (depende do token ser aceito) | — |
| `tools/token_bridge.py` | ⏳ Não testado contra o device real | — |
| Publicação no GitHub | ⏳ Repo local (`git init` feito); sem commit, sem remote, sem push | — |

## O que já foi corrigido nesta rodada de bring-up

Três bugs reais encontrados e corrigidos **só através de teste em hardware** (não
detectáveis por leitura de código):

1. `CONFIG_LV_CONF_SKIP=y` no `sdkconfig.defaults` fazia o LVGL **ignorar**
   `main/lv_conf.h` (nome da flag é contraintuitivo) → corrigido para
   `# CONFIG_LV_CONF_SKIP is not set` + `LV_CONF_PATH` global via CMake.
2. Stack overflow na task `"main"` (`app_main()` fazendo display+Wi-Fi+mDNS+HTTP
   server+SD tudo inline) → `CONFIG_ESP_MAIN_TASK_STACK_SIZE=12288`.
3. Stack overflow na task do `esp_http_server` (handler de `POST /token` chama
   `fetchUsage()`, uma requisição HTTPS/TLS completa) → `config.stack_size = 8192`
   em `onboarding_server_start()`.

Detalhes completos de cada um em
`firmware/panlee_sc01_plus/REFERENCIA-HARDWARE-LVGL.md` (seção "Armadilhas
conhecidas").

## Escopo explicitamente fora deste porte

- Mouse BLE HID, slideshow de JPEG, servidor de arquivos genérico (recursos de
  bring-up do pacote inicial da placa, não relacionados ao produto).
- RS485, áudio.
- Histórico em partição LittleFS interna (decisão: usar o microSD já validado nesta
  placa em vez disso).
