# Registro de builds — Claude Usage Stick (Panlee SC01 Plus)

Registre aqui cada build gravado em hardware real: versão (`FW_VERSION` em
`main/config.h`), a alteração isolada testada, o hash do commit e a evidência
(visual e/ou serial). Isso segue a mesma disciplina de "uma alteração isolada por
build" já usada no bring-up desta placa — como este ambiente de desenvolvimento não
tem acesso físico à placa, a validação em hardware é sempre feita pelo
desenvolvedor/usuário, e este arquivo é o registro combinado dessas validações.

| Data | Versão (`FW_VERSION`) | Alteração testada | Commit | Evidência |
|---|---|---|---|---|
| 2026-07-29 | `1.0.0-sc01plus` | 1º `idf.py build` completo (Fases 1–4) | `aed4ebe` | ❌ `error: 'lv_font_montserrat_48' undeclared` — `CONFIG_LV_CONF_SKIP` habilitado por default do Kconfig do componente `lvgl/lvgl`, fazendo `main/lv_conf.h` ser ignorado |
| 2026-07-29 | `1.0.0-sc01plus` | `# CONFIG_LV_CONF_SKIP is not set` explícito em `sdkconfig.defaults` | `aed4ebe` | ❌ `fatal error: lv_conf.h: No such file or directory` — arquivos internos do `lvgl__lvgl` (ex. `lv_group.c`) não têm `-Imain` no include path |
| 2026-07-29 | `1.0.0-sc01plus` | `LV_CONF_PATH` global via `idf_build_set_property` em `main/CMakeLists.txt` | `aed4ebe` | ❌ `CMake Error: Unknown CMake command "idf_build_set_property"` — chamado na passada leve (`CMAKE_BUILD_EARLY_EXPANSION`) |
| 2026-07-29 | `1.0.0-sc01plus` | Guard `if(NOT CMAKE_BUILD_EARLY_EXPANSION)` ao redor do `idf_build_set_property` | `aed4ebe` | ❌ Compilou tudo, falhou no link: `undefined reference to 'ui_loading'` (função nunca implementada no porte) |
| 2026-07-29 | `1.0.0-sc01plus` | `ui_loading()` implementado em `ui_common.c` | `aed4ebe` | ✅ Build + link OK — `claude_usage_panlee_sc01_plus.bin`, 0x1ebba0 bytes, 52% da partição livre |
| 2026-07-29 | `1.0.0-sc01plus` | 1º flash + monitor em hardware real (COM3) | `aed4ebe` | ⚠️ LCD/Wi-Fi/SD/mDNS inicializam certo; `***ERROR*** A stack overflow in task main has been detected` logo após montar o SD (antes de `apply_brightness()` — tela ficava apagada) |
| 2026-07-29 | `1.0.0-sc01plus` | `CONFIG_ESP_MAIN_TASK_STACK_SIZE=12288` em `sdkconfig.defaults` | `aed4ebe` | ✅ Boot estável: Wi-Fi conectado (`MateWeb_IoT`, IP `192.168.10.115`), microSD montado, mDNS + servidor de onboarding ativos, tela de token exibida |
| 2026-07-29 | `1.0.0-sc01plus` | Usuário colou token real pela 1ª vez | `aed4ebe` | ❌ Tela não saía de "aguardando o token"; serial mostrou reset com `Saved PC: _WindowOverflow8` — stack overflow na task do `esp_http_server` (handler de `POST /token` chama `fetchUsage()`, uma requisição HTTPS/TLS completa) |
| 2026-07-29 | `1.0.0-sc01plus` | `httpd_config_t.stack_size = 8192` em `onboarding_server_start()` | `aed4ebe` | ✅ Sem crash; navegador mostrou "Token inválido" — rejeição real da API (erro classe 401), não mais falha de firmware. Causa real confirmada depois: o token colado era uma API key (`sk-ant-api03-...`), não um token OAuth (`sk-ant-oat01-...`) |
| 2026-07-29 | `1.0.0-sc01plus` | Tela "Sobre" atualizada com crédito de adaptação (Marcelo Sobral) além do original (Benevid Felix) | `aed4ebe` | ✅ Build + flash OK (mudança cosmética, sem impacto funcional) |
| 2026-07-29 | `1.0.0-sc01plus` | Token OAuth real (`sk-ant-oat01-...`) colado no onboarding | `aed4ebe` | ✅ `HTTP 200`, `5h:6% 7d:69%`, dashboard alcançado com dados reais |
| 2026-07-29 | `1.0.0-sc01plus` | Toggle "Solicitar PIN no boot: Sim/Não" nos Ajustes (`FIXED_PIN`, `g_pinRequired`, `ST_SETUP_PIN` com origem em Ajustes) | `aed4ebe` | ✅ Build + flash OK; comportamento confirmado (desligar exige 2 toques, religar exige definir PIN de verdade) |
| 2026-07-29 | `1.0.0-sc01plus` | `esp_wifi_set_ps(WIFI_PS_NONE)` + `CONFIG_LWIP_SNTP_MAX_SERVERS=2` | `aed4ebe` | ✅ SNTP passou a inicializar (Tendência/Heatmap deixaram de ficar vazios); sem mais falhas recorrentes de TLS após transição de power-save |
| 2026-07-29 | `1.0.0-sc01plus` | Relógio no cabeçalho do dashboard (`g_hdrClock`/`fmt_now`) + servidor NTP editável nos Ajustes (`g_ntpServer`, NVS `ntpsrv`, tela `ST_NTP_SERVER`) | `5254825` | ✅ Build + flash OK; log serial `TIME_SYNC: SNTP iniciado (pool.ntp.br, time.cloudflare.com)` confirma leitura do valor configurável; usuário confirmou visualmente relógio e tela de Ajustes funcionando |

> Preencha uma linha nova a cada gravação. Se uma alteração falhar na validação,
> registre também — evita repetir o mesmo teste sem saber que já foi tentado. A
> coluna "Commit" reflete o commit em que a alteração foi consolidada — várias linhas
> de depuração de uma mesma sessão podem compartilhar o mesmo hash quando foram
> corrigidas antes do primeiro commit existir.
