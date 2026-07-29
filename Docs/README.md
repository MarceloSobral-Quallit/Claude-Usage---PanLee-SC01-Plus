# Escopo funcional e estado atual — Claude Usage Stick (Panlee SC01 Plus)

> Este documento descreve **o que está entregue e em que estado de validação**, para
> orientar o roadmap em `DEV_PLAYBOOK.md`. Para a descrição de produto (o que o
> dispositivo faz, telas, screenshots conceituais), ver o [`README.md`](../README.md)
> da raiz. Atualizado em 2026-07-29.

## O que é

Porte completo do firmware **Claude Usage Stick** (originalmente Arduino/ESP32-S3 com
display QSPI AXS15231B) para a placa **Smart Panlee / Wireless-Tag SC01 Plus**
(ESP32-S3, LCD ST7796UI via barramento i80, touch FT6336U), reescrito em **ESP-IDF
puro + LVGL 9.2** (`firmware/panlee_sc01_plus/`). Publicado em
`https://github.com/MarceloSobral-Quallit/Claude-Usage---PanLee-SC01-Plus`.

## Estado atual (validado em hardware real, COM3)

| Área | Estado | Evidência |
|---|---|---|
| Build (ESP-IDF local ao repo) | ✅ Compila do zero (`idf.py build`) | `docs/REGISTRO_DE_BUILDS.md` |
| LCD (ST7796/i80, paisagem 480×320) | ✅ Inicializa, contrato de cor correto | log serial: `LCD initialized at 10 MHz, logical 480x320` |
| Touch (FT6336U) | ✅ Confirmado interativamente pelo usuário (navegação em Ajustes, teclado da tela de servidor NTP) | confirmado visualmente |
| Wi-Fi (onboarding + auto-connect) | ✅ Conecta com rede pré-semeada (`wifi_cred.txt`, opcional) ou por onboarding | log serial: `WIFI_MGR: connected; IP=...` |
| microSD (montagem, histórico) | ✅ Monta, lista arquivos, grava/lê `HISTORY.DAT` | log serial: `historico carregado: N amostras` |
| mDNS + servidor de onboarding | ✅ `claude-stick.local`, porta 80 ativa | log serial |
| Onboarding — tela de token (UI) | ✅ Mostra IP, aguarda POST /token | confirmado visualmente |
| Onboarding — validação do token (API) | ✅ Token OAuth (`sk-ant-oat01-...`) aceito, `HTTP 200` | log serial: `API: HTTP 200 ... 5h:6% 7d:69%` |
| PIN (tela + criptografia do token) | ✅ Definição/verificação do PIN funcionando; toggle **"Solicitar PIN no boot"** implementado e testado | confirmado visualmente |
| Dashboard (4 tiles: Agora/Modelos/Tendência/Ritmo) | ✅ Alcançado e populado com dados reais | confirmado visualmente |
| Relógio do dispositivo no cabeçalho | ✅ HH:MM:SS ao vivo, referência de que o SNTP sincronizou | confirmado visualmente pelo usuário |
| Ajustes → Servidor NTP editável | ✅ Tela com teclado, persiste em NVS, aplica no próximo boot | confirmado visualmente pelo usuário + log serial (`SNTP iniciado (pool.ntp.br, ...)`) |
| `tools/token_bridge.py` | ⏳ Não testado contra o device real | — |
| Publicação no GitHub | ✅ 3 commits, remote configurado, push confirmado pelo usuário | `git log` / repositório remoto |

## Bugs encontrados e corrigidos só em hardware real (não detectáveis por leitura de código)

Seis bugs confirmados só através de teste físico na placa — detalhes completos em
`firmware/panlee_sc01_plus/REFERENCIA-HARDWARE-LVGL.md` (seção "Armadilhas
conhecidas") e no histórico de `docs/REGISTRO_DE_BUILDS.md`:

1. `CONFIG_LV_CONF_SKIP=y` no `sdkconfig.defaults` fazia o LVGL **ignorar**
   `main/lv_conf.h` (nome da flag é contraintuitivo) → corrigido para
   `# CONFIG_LV_CONF_SKIP is not set` + `LV_CONF_PATH` global via CMake.
2. Include path do componente `lvgl/lvgl` não alcançava `main/lv_conf.h` →
   corrigido com `LV_CONF_PATH` global (guardado por `CMAKE_BUILD_EARLY_EXPANSION`).
3. Stack overflow na task `"main"` (`app_main()` fazendo display+Wi-Fi+mDNS+HTTP
   server+SD tudo inline) → `CONFIG_ESP_MAIN_TASK_STACK_SIZE=12288`.
4. Stack overflow na task do `esp_http_server` (handler de `POST /token` chama
   `fetchUsage()`, uma requisição HTTPS/TLS completa) → `config.stack_size = 8192`
   em `onboarding_server_start()`.
5. SNTP nunca inicializava (`CONFIG_LWIP_SNTP_MAX_SERVERS` menor que o número de
   servidores configurados) → `CONFIG_LWIP_SNTP_MAX_SERVERS=2`.
6. Power-save do Wi-Fi corrompendo handshakes TLS consecutivos → corrigido com
   `esp_wifi_set_ps(WIFI_PS_NONE)`.

## Funcionalidades adicionadas além do porte 1:1 (ver `DIFERENCAS_PROJETO_ORIGINAL.md`)

- Toggle **"Solicitar PIN no boot: Sim/Não"** nos Ajustes.
- Validação do prefixo do token (`sk-ant-oat01-`) antes de gastar uma chamada de rede.
- Relógio do dispositivo no cabeçalho do dashboard (referência/certificação do NTP).
- **Ajustes → Servidor NTP** editável (hostname/IP), persistido em NVS.
- Ambiente de build 100% local ao repositório (`.idf-env/`).

## Escopo explicitamente fora deste porte

- Mouse BLE HID, slideshow de JPEG, servidor de arquivos genérico (recursos de
  bring-up do pacote inicial da placa, não relacionados ao produto).
- RS485, áudio.
- Histórico em partição LittleFS interna (decisão: usar o microSD já validado nesta
  placa em vez disso).

## Pendências conhecidas (não bloqueantes)

- `tools/token_bridge.py` nunca foi testado contra o device real (Fase C do roadmap).
- `CLAUDE_CODE_USER_AGENT` (`main/config.h`) permanece com o valor herdado do projeto
  original (`claude-code/2.1.5`), nunca confirmado contra um `claude --version` real —
  deixou de ser bloqueante quando se confirmou que a rejeição original do token era por
  tipo de chave errado (API key em vez de token OAuth), não por User-Agent.
- Pasta `Docs/` está versionada no Git com **D maiúsculo**, enquanto toda a
  documentação (inclusive este arquivo) referencia `docs/` minúsculo nos links —
  inofensivo no Windows (case-insensitive), mas quebra links relativos ao navegar o
  repositório no GitHub (case-sensitive). Ver `DEV_PLAYBOOK.md` § pendências.
