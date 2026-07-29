# Claude Usage Stick — Panlee SC01 Plus (ESP32-S3 + LVGL 9)

Um gadget de mesa que mostra o **uso do rate-limit do Claude Code** em tempo real
numa tela touch de 3.5". Sem computador, sem app, sem nuvem: o dispositivo consulta
a API da Anthropic diretamente, lê o uso direto dos headers da resposta e renderiza
tudo num dashboard — com os mascotes animados **Clawd**, gráfico de tendência de
uso e heatmap por hora.

> Porte do projeto original **Claude Usage Stick** (display QSPI AXS15231B,
> Arduino/arduino-cli) para a placa **Smart Panlee / Wireless-Tag SC01 Plus**
> (ESP32-S3, LCD ST7796UI via barramento i80, touch FT6336U), em **ESP-IDF puro** —
> ver [O que veio do projeto original](#o-que-veio-do-projeto-original).

> A interface na tela está em português (idioma padrão) com alternância para
> inglês nos Ajustes. Este README documenta em português.

---

## Telas

Navegação por **swipe** (os pontinhos embaixo mostram sua posição). A **engrenagem**
abre os Ajustes. A barra fina coral abaixo do cabeçalho conta regressivamente até a
próxima atualização — tocar nela atualiza na hora.

1. **Agora** — dois cards grandes (janela de **5 horas** e **semana**), cada um com
   percentual grande, um **medidor de 18 segmentos** que desliza de verde a vermelho
   conforme a janela enche, contagem regressiva para o reset e horário local do reset.
2. **Modelos** — os 4 mascotes Clawd (Haiku/Sonnet/Opus/Fable) com **status real**,
   vindo de uma sonda contra a API (1 modelo por ciclo, revezando) + incidentes de
   `status.claude.com`.
3. **Janela de 5h** — gráfico com o histórico real (linha sólida) e a **projeção**
   pontilhada de esgotamento no ritmo atual.
4. **Ritmo por hora** — 24 barras mostrando em quais horas o consumo é maior, com
   filtro **Hoje / 7d / 30d / Tudo** (heatmap persistido no microSD).
5. **Momentos** — animação em tela cheia ao cruzar 25/50/70/100% de qualquer janela.

**Ajustes**: atualizar agora, intervalo de atualização, slideshow, fuso horário,
brilho, Wi-Fi, trocar token, idioma, sobre, apagar tudo (reset de fábrica).

---

## Hardware

| | |
|---|---|
| Placa | **Smart Panlee / Wireless-Tag SC01 Plus** `ZX3D50CE08S-V16-USRC` |
| Chip | ESP32-S3 |
| Display | **ST7796UI**, 320×480 físico, barramento i80/MCU 8080 de 8 bits, usado em **paisagem 480×320** |
| Touch | **FT6336U/FT5x06** capacitivo, I²C `0x38` |
| Flash / PSRAM | 16 MiB flash · 2 MiB PSRAM (Quad; não usada pelo pipeline gráfico) |
| Cartão | microSD SPI (histórico/heatmap + log) |

> Onde comprar a placa: [Smart Panlee SC01 Plus no AliExpress](https://pt.aliexpress.com/item/1005006050379552.html?channel=twinner).

Pinagem completa, contrato de cor e a estratégia de inicialização do LVGL 9 nesta
placa estão em
[`firmware/panlee_sc01_plus/REFERENCIA-HARDWARE-LVGL.md`](firmware/panlee_sc01_plus/REFERENCIA-HARDWARE-LVGL.md)
— **leia antes de mexer em display/touch**. Mais documentação de bring-up em
[`docs/`](docs/).

---

## Como funciona (e o token)

O gadget faz um `POST` **mínimo** (`max_tokens: 1`) para
`https://api.anthropic.com/v1/messages` e **não usa o corpo da resposta** — lê o uso
direto dos headers:

```
anthropic-ratelimit-unified-status                allowed | allowed_warning | rejected
anthropic-ratelimit-unified-5h-utilization        0–1   (vira o % da janela de 5h)
anthropic-ratelimit-unified-5h-reset              epoch
anthropic-ratelimit-unified-7d-utilization        0–1   (janela de 7 dias)
anthropic-ratelimit-unified-7d-reset              epoch
anthropic-ratelimit-unified-representative-claim  five_hour | seven_day
anthropic-ratelimit-unified-fallback-percentage
anthropic-ratelimit-unified-overage-status / -overage-disabled-reason
```

A saúde dos modelos combina `status.claude.com/api/v2/incidents/unresolved.json`
com uma **sonda por modelo**: a cada ciclo de atualização o device manda um request
`max_tokens: 1` para o próximo modelo da rotação (Haiku → Sonnet → Opus → Fable) e
registra código HTTP + latência.

### Tokens por sessão (bridge opcional)

A API não expõe contagem de tokens para contas de assinatura — os números reais
vivem nos **transcripts locais do Claude Code** (`~/.claude/projects/**/*.jsonl`).
[`tools/token_bridge.py`](tools/token_bridge.py) (stdlib puro) fecha essa lacuna:
pergunta ao device a janela atual (`GET http://claude-stick.local/window`), soma os
tokens dos transcripts desde o início da janela e envia de volta
(`POST /tokens`).

```bash
python3 tools/token_bridge.py               # um envio
python3 tools/token_bridge.py --loop 120    # reenvia a cada 2 min
```

O device se anuncia via mDNS como **`claude-stick.local`**.

### Gerando o token (`claude setup-token`)

Com o **Claude Code** instalado e logado na sua assinatura (**Pro** ou **Max**):

```bash
claude setup-token
```

Abre um fluxo **OAuth** no navegador; você recebe um token de longa duração
(`sk-ant-oat01-…`). O gadget envia os mesmos headers que o Claude Code envia
(`anthropic-beta: oauth-2025-04-20` + User-Agent), então uma chamada mínima
(`max_tokens: 1`) é aceita normalmente — consumo de quota é desprezível.

> ⚠️ **O token tem que vir do CLI, com o prefixo certo.** Só funciona o token gerado
> por `claude setup-token` — que começa com **`sk-ant-oat01-`**. Uma **chave de API**
> comum do console da Anthropic (`sk-ant-api03-…`) **não funciona aqui** e é
> recusada: essa chamada depende do fluxo OAuth do próprio Claude Code, não de uma
> API key normal. O firmware já valida esse prefixo antes de tentar a rede e mostra
> o motivo exato da recusa na página web do onboarding.

> O token é digitado **uma vez** (pelo navegador, na etapa de onboarding) e fica
> guardado **cifrado** no dispositivo (AES-256-GCM, chave derivada de um PIN de 4
> dígitos).

---

## Build & flash

O ambiente ESP-IDF fica **dentro deste repositório** (`.idf-env/`, fora do git) —
nada é instalado no Python global nem em pastas fora do repo. Ver
[`docs/REQUISITOS_COMPILACAO.md`](docs/REQUISITOS_COMPILACAO.md) para requisitos e
detalhes.

```powershell
# Uma vez: clona o ESP-IDF 5.5.x e instala o toolchain do ESP32-S3 em .idf-env/
.\tools\setup_esp_idf_env.ps1

# Em toda sessão nova do PowerShell em que for compilar:
. .\tools\activate-esp-idf.ps1

cd firmware\panlee_sc01_plus
idf.py set-target esp32s3
idf.py build
idf.py -p COM3 flash monitor
```

A primeira compilação baixa as dependências gerenciadas (LVGL 9.2.2, driver
`esp_lcd_st7796`, `mdns`) declaradas em `main/idf_component.yml`.

> Já tem um ESP-IDF 5.5.x instalado globalmente (ex.: atalho "ESP-IDF 5.5
> PowerShell")? Pode usá-lo direto em vez do script acima — os dois convivem sem
> conflito.

> Se o build reclamar de `lv_conf.h not found` ou de uma flag de Kconfig diferente
> de `CONFIG_LV_CONF_SKIP`, veja a seção de "armadilhas" em
> `firmware/panlee_sc01_plus/REFERENCIA-HARDWARE-LVGL.md`.

### Wi-Fi

Onboarding **100% pela tela** — não precisa recompilar para trocar de rede:

1. Toque na sua rede na lista, digite a senha no teclado da tela.
2. Cole o token OAuth pelo navegador (o device mostra o próprio IP).
3. Defina um PIN de 4 dígitos (pedido a cada boot seguinte).

`wifi_cred.txt` na raiz do repositório (fora do controle de versão) é **opcional** e
serve só de conveniência para desenvolvimento: se presente, pré-semeia uma rede na
primeira inicialização, para não precisar digitar a senha na tela a cada gravação.
Nunca é obrigatório e nunca deve ser versionado.

---

## Segurança

- Token guardado **cifrado** (AES-256-GCM; chave derivada do PIN via SHA-256,
  10.000 rounds). O PIN nunca é armazenado — um PIN errado falha a verificação do
  tag do GCM.
- Após 10 tentativas erradas, as credenciais são **apagadas** (lockout dobra a cada
  falha).
- Ajustes → **"Solicitar PIN no boot"** pode ser desligado para pular a digitação do
  PIN a cada boot — mas isso troca a chave de cifra por um PIN fixo interno
  conhecido, reduzindo a proteção do token a apenas ofuscação contra um dump da
  flash (não protege contra o uso direto do aparelho já ligado). Religar exige
  definir um PIN de verdade de novo.
- TLS via bundle de CAs embutido do mbedTLS (`esp_crt_bundle_attach`) — nenhum
  certificado customizado no repositório.
- `wifi_cred.txt` está no `.gitignore` — nenhum segredo vai para o git.

---

## O que veio do projeto original

Este firmware é um **porte** do Claude Usage Stick original (Arduino, display QSPI
AXS15231B) para a placa Panlee SC01 Plus, em **ESP-IDF puro**. Resumo abaixo;
comparação completa (com mapa arquivo por arquivo) em
[`docs/DIFERENCAS_PROJETO_ORIGINAL.md`](docs/DIFERENCAS_PROJETO_ORIGINAL.md).

**Reaproveitado (quase literalmente):**

- Toda a **UI em LVGL 9** — telas, mascotes Clawd, medidores segmentados, gráfico de
  tendência, heatmap, animações de limiar — porque a UI original já usava a API do
  LVGL 9 (`lv_display_create`, `lv_image_create`, `lv_buttonmatrix`), que se mantém
  quase idêntica nesta placa.
- A lógica de **leitura do rate-limit pelos headers** `anthropic-ratelimit-unified-*`.
- A **sonda de saúde por modelo** e o cruzamento com `status.claude.com`.
- A **criptografia do token** (AES-256-GCM + chave derivada do PIN).
- O conceito de produto e os **mascotes Clawd**.

**Reescrito para esta placa:**

- Driver de display/touch: barramento **i80/ST7796** + **FT6336U** (em vez de
  QSPI/AXS15231B), com buffer parcial em RAM interna (em vez de full-frame em PSRAM
  — ver `REFERENCIA-HARDWARE-LVGL.md`).
- Rede/TLS/HTTP: `esp_http_client` + bundle de certificados do ESP-IDF (em vez de
  `WiFiClientSecure`/`HTTPClient` do Arduino).
- Onboarding Wi-Fi e servidor web: `esp_wifi` + NVS e `esp_http_server` + `mdns` (em
  vez de `WiFiManager`/`WebServer`/`ESPmDNS` do Arduino).
- Histórico/heatmap: arquivo `HISTORY.DAT` no **microSD** (em vez de LittleFS
  interno) — decisão deliberada para reaproveitar o cartão já validado nesta placa.
- Busca de rede roda numa **task FreeRTOS dedicada**, mantendo a tela responsiva
  durante a atualização (o firmware original, de núcleo único em Arduino,
  bloqueava a UI durante o fetch).

---

## Repositório

```
firmware/panlee_sc01_plus/     # firmware ESP-IDF
  main/                        # bsp_*, api/status/crypto/storage, wifi_manager,
                                # onboarding_server, ui_*, app_state, app_main
  REFERENCIA-HARDWARE-LVGL.md  # pinagem, contrato de cor, inicializacao do LVGL 9
docs/                          # referencia da placa, regras de LCD, registro de builds
tools/                         # token_bridge.py, gen_logo_assets.py, gen_mockups.py
assets/brand/                  # SVGs oficiais do Claude Code (mascotes/logotipo)
```

## Créditos

Baseado no projeto original **Claude Usage Stick**, de Benevid Felix. Este firmware
foi adaptado para a placa Smart Panlee SC01 Plus (ESP32-S3, LVGL 9 sobre
ST7796UI/i80) por Marcelo Sobral. Não é um produto oficial da Anthropic.

## Documentação e processo de desenvolvimento

Estado atual do projeto, decisões técnicas e roadmap: ver [`docs/`](docs/), a
começar por [`docs/INDEX.md`](docs/INDEX.md). As alterações hardware-sensíveis
seguem o ciclo descrito em
[`docs/protocolo-ciclico-publicacao-exclusao.md`](docs/protocolo-ciclico-publicacao-exclusao.md).
