# Registro de builds — Claude Usage Stick (Panlee SC01 Plus)

Registre aqui cada build gravado em hardware real: versão (`FW_VERSION` em
`main/config.h`), a alteração isolada testada, o hash do commit e a evidência
(visual e/ou serial). Isso segue a mesma disciplina de "uma alteração isolada por
build" já usada no bring-up desta placa — como este ambiente de desenvolvimento não
tem acesso físico à placa, a validação em hardware é sempre feita pelo
desenvolvedor/usuário, e este arquivo é o registro combinado dessas validações.

| Data | Versão (`FW_VERSION`) | Alteração testada | Commit | Evidência |
|---|---|---|---|---|
| 2026-07-29 | `1.0.0-sc01plus` | 1º `idf.py build` completo (Fases 1–4) | sem commit ainda | ❌ `error: 'lv_font_montserrat_48' undeclared` — `CONFIG_LV_CONF_SKIP` habilitado por default do Kconfig do componente `lvgl/lvgl`, fazendo `main/lv_conf.h` ser ignorado |
| 2026-07-29 | `1.0.0-sc01plus` | `# CONFIG_LV_CONF_SKIP is not set` explícito em `sdkconfig.defaults` | sem commit ainda | ❌ `fatal error: lv_conf.h: No such file or directory` — arquivos internos do `lvgl__lvgl` (ex. `lv_group.c`) não têm `-Imain` no include path |
| 2026-07-29 | `1.0.0-sc01plus` | `LV_CONF_PATH` global via `idf_build_set_property` em `main/CMakeLists.txt` | sem commit ainda | ❌ `CMake Error: Unknown CMake command "idf_build_set_property"` — chamado na passada leve (`CMAKE_BUILD_EARLY_EXPANSION`) |
| 2026-07-29 | `1.0.0-sc01plus` | Guard `if(NOT CMAKE_BUILD_EARLY_EXPANSION)` ao redor do `idf_build_set_property` | sem commit ainda | ❌ Compilou tudo, falhou no link: `undefined reference to 'ui_loading'` (função nunca implementada no porte) |
| 2026-07-29 | `1.0.0-sc01plus` | `ui_loading()` implementado em `ui_common.c` | sem commit ainda | ✅ Build + link OK — `claude_usage_panlee_sc01_plus.bin`, 0x1ebba0 bytes, 52% da partição livre |
| 2026-07-29 | `1.0.0-sc01plus` | 1º flash + monitor em hardware real (COM3) | sem commit ainda | ⚠️ LCD/Wi-Fi/SD/mDNS inicializam certo; `***ERROR*** A stack overflow in task main has been detected` logo após montar o SD (antes de `apply_brightness()` — tela ficava apagada) |
| 2026-07-29 | `1.0.0-sc01plus` | `CONFIG_ESP_MAIN_TASK_STACK_SIZE=12288` em `sdkconfig.defaults` | sem commit ainda | ✅ Boot estável: Wi-Fi conectado (`MateWeb_IoT`, IP `192.168.10.115`), microSD montado, mDNS + servidor de onboarding ativos, tela de token exibida |
| 2026-07-29 | `1.0.0-sc01plus` | Usuário colou token real pela 1ª vez | sem commit ainda | ❌ Tela não saía de "aguardando o token"; serial mostrou reset com `Saved PC: _WindowOverflow8` — stack overflow na task do `esp_http_server` (handler de `POST /token` chama `fetchUsage()`, uma requisição HTTPS/TLS completa) |
| 2026-07-29 | `1.0.0-sc01plus` | `httpd_config_t.stack_size = 8192` em `onboarding_server_start()` | sem commit ainda | ✅ Sem crash; navegador mostrou "Token inválido" — rejeição real da API (erro classe 401), não mais falha de firmware. Pendente: `User-Agent` desatualizado (`main/config.h`) — ver `DEV_PLAYBOOK.md` § Fase A |
| 2026-07-29 | `1.0.0-sc01plus` | Tela "Sobre" atualizada com crédito de adaptação (Marcelo Sobral) além do original (Benevid Felix) | sem commit ainda | ✅ Build + flash OK (mudança cosmética, sem impacto funcional) |

> Preencha uma linha nova a cada gravação. Se uma alteração falhar na validação,
> registre também — evita repetir o mesmo teste sem saber que já foi tentado. A
> coluna "Commit" fica "sem commit ainda" enquanto o repositório não tiver o
> primeiro commit (ver `DEV_PLAYBOOK.md` § Fase D); atualizar com o hash real assim
> que existir.
