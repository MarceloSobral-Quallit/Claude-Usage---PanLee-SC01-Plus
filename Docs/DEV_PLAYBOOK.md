# Dev Playbook — Claude Usage Stick (Panlee SC01 Plus)

Decisões técnicas registradas + roadmap de próximos passos. Ver `docs/README.md` para
o estado atual e `protocolo-ciclico-publicacao-exclusao.md` para a disciplina de
validação usada a cada rodada.

## Decisões técnicas e por quê

| Decisão | Alternativa considerada | Por quê |
|---|---|---|
| **LVGL 9.2** (resolve para 9.5.x via `^9.2.2`) | LVGL 8.3.11 (pinado pelo pacote inicial da placa) | A UI original já usa API exclusiva do v9 (`lv_display_create`, `lv_image_create`, `lv_buttonmatrix`); reaproveita ~90% do código de UI quase literal. |
| Histórico/heatmap em **arquivo no microSD** (`HISTORY.DAT`) | Partição LittleFS interna | Decisão do usuário: reaproveitar o slot SD já validado nesta placa em vez de criar uma partição nova. Custo: histórico fica vazio se o cartão for removido. |
| Ambiente ESP-IDF **local ao repositório** (`.idf-env/`, via `tools/setup_esp_idf_env.ps1`) | Instalação global (`~/esp`, `~/.espressif`) | Pedido explícito do usuário: nada deve alterar o ambiente da máquina fora do repo; cada checkout é reprodutível sozinho. |
| Busca de rede (`api.c`/`status.c`) roda em **task FreeRTOS dedicada** | Bloquear a task do LVGL (como o firmware original em Arduino fazia) | Mantém a tela responsiva durante o fetch HTTPS (~1-2s); a task de rede só escreve em variáveis simples, quem aplica na LVGL é a task principal. |
| Task stack sizing: **`CONFIG_ESP_MAIN_TASK_STACK_SIZE=12288`** e **`httpd_config_t.stack_size=8192`** no servidor de onboarding | Defaults do ESP-IDF (3584 / 4096 bytes) | Confirmado em hardware: qualquer task que chame `esp_http_client`/TLS precisa de pilha generosa. Ver `REFERENCIA-HARDWARE-LVGL.md` § armadilhas. |
| `LV_CONF_PATH` definido globalmente via `idf_build_set_property` (guardado por `CMAKE_BUILD_EARLY_EXPANSION`) em `main/CMakeLists.txt` | Depender só de `-Imain` (`LV_CONF_INCLUDE_SIMPLE`) | O componente `lvgl/lvgl` só adiciona `managed_components/` ao seu próprio include path, não `main/` — arquivos internos do LVGL não achavam `lv_conf.h` sem isso. |

## Roadmap (gerado via `/roadmap` em 2026-07-29)

### Contexto entendido

Firmware já portado (Fases 1–4 concluídas) e em bring-up ativo em hardware real
(COM3). Build e ambiente local funcionam; três bugs de hardware já corrigidos
(LV_CONF_SKIP, dois stack overflows). Bloqueio atual: a API da Anthropic rejeita o
token colado na tela de onboarding com um erro classe 401, suspeita principal é o
`User-Agent: claude-code/2.1.5` (herdado do projeto original) estar desatualizado
para o mecanismo de "disfarce" de OAuth do Claude Code.

### Suposições

- O token colado pelo usuário foi gerado corretamente via `claude setup-token` (não
  confirmado ainda se veio de uma instalação atual do Claude Code CLI).
- A placa permanece conectada via COM3 e na mesma rede Wi-Fi (`MateWeb_IoT`) para os
  próximos testes.
- Push para o GitHub só deve acontecer mediante pedido explícito do usuário (nunca
  automático).

### Fase A — Resolver a rejeição do token pela API

Objetivo: aceitar um token OAuth válido no onboarding sem erro 401.

Tarefas:
- Obter a versão real do Claude Code CLI (`claude --version`) — na máquina onde o
  token foi gerado, ou instalando o CLI nesta máquina (`npm install -g
  @anthropic-ai/claude-code`), mediante confirmação do usuário.
- Atualizar `CLAUDE_CODE_USER_AGENT` em `main/config.h` para o valor real.
- Opcional: melhorar `api.c` para logar o `status_code` mesmo quando
  `esp_http_client_perform()` retorna erro no caminho de 401 (hoje esse caso cai em
  `http_err_<N>` genérico em vez de `auth_failed`), facilitando diagnóstico futuro.

Validação:
- Recompilar, gravar, colar o mesmo token na tela de onboarding; confirmar que a
  tela avança para "definir PIN" (`ST_SETUP_PIN`) em vez de mostrar "Token inválido".

### Fase B — Validar o fluxo completo em hardware

Objetivo: confirmar visualmente cada tela e interação com o dedo.

Tarefas:
- Definir o PIN (duas vezes) e confirmar que volta a pedir o PIN em um novo boot.
- Confirmar touch em todas as telas (PIN, Wi-Fi, dashboard, ajustes).
- Confirmar as 4 tiles do dashboard (Agora, Modelos, Janela de 5h, Ritmo) com swipe.
- Forçar cruzar um limiar (25/50/70/100%) se possível, ou revisar a lógica visualmente.
- Testar Ajustes: brilho, intervalo de atualização, fuso horário, idioma, apagar tudo.

Validação:
- Nenhum crash/reset inesperado durante a navegação; `docs/REGISTRO_DE_BUILDS.md`
  atualizado com a evidência.

### Fase C — Validar `tools/token_bridge.py` (opcional)

Objetivo: confirmar a ponte de tokens por sessão contra o device real.

Tarefas:
- Rodar `python3 tools/token_bridge.py --host <ip-do-device>` (ou via mDNS
  `claude-stick.local`, se a resolução mDNS funcionar na máquina do usuário).
- Confirmar que `GET /window` e `POST /tokens` respondem como esperado e que a tela
  "Agora" mostra a linha de tokens da janela.

Validação:
- Saída do script sem erro; label de tokens aparece no dashboard dentro de 15 min.

### Fase D — Preparar publicação no GitHub

Objetivo: publicar o repositório em
`https://github.com/MarceloSobral-Quallit/Claude-Usage---PanLee-SC01-Plus` — **só
mediante pedido explícito do usuário**, uma etapa de cada vez.

Tarefas (cada uma exige confirmação separada antes de executar):
- Revisar `git status`/`git diff` e confirmar que nenhum segredo (`wifi_cred.txt`,
  tokens) está staged.
- Primeiro commit.
- Configurar o remote (`git remote add origin ...`).
- Push (`git push -u origin main`).

Validação:
- `git log` mostra o commit esperado; repositório no GitHub reflete o conteúdo local.

### Fase E — Manutenção contínua

Objetivo: manter a disciplina de registro a cada mudança hardware-sensível.

Tarefas:
- Toda alteração em display/touch/rede/tasks: registrar em
  `docs/REGISTRO_DE_BUILDS.md` (versão, alteração isolada, hash, evidência) — ver
  `protocolo-ciclico-publicacao-exclusao.md`.
- Reavaliar periodicamente se `tools/` (scripts Python) precisa de um venv dedicado
  (hoje só documentado para o ambiente ESP-IDF em si).

## Riscos e mitigações

- **Risco**: o `User-Agent` correto pode não resolver sozinho o 401 (pode haver
  outro header/mecanismo do Claude Code que mudou desde o projeto original).
  **Mitigação**: testar com `curl` fora do device primeiro (mesmo payload/headers)
  para isolar se é problema de API/token ou de implementação do firmware.
- **Risco**: push para o GitHub é uma ação visível/irreversível em repositório
  público. **Mitigação**: sempre pedir confirmação explícita antes, nunca incluir no
  escopo de uma tarefa maior implicitamente.

## Perguntas pendentes

- Instalar o Claude Code CLI nesta máquina, ou o usuário vai checar a versão em
  outra máquina?

## Próximo passo recomendado

Fechar a Fase A (User-Agent) assim que a versão do Claude Code CLI for confirmada.
