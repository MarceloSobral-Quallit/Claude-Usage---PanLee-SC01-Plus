# Referência de hardware e LVGL — Panlee SC01 Plus

Documento de referência única para esta placa e este firmware (Claude Usage Stick).
Consolida o que estava espalhado em `docs/REFERENCIA_PLACA_SC01_PLUS.md`,
`docs/DICIONARIO_CONFIGURACAO_PLACA.md` e `docs/REGRAS_DESENVOLVIMENTO_LCD.md` do
pacote inicial da placa, mais o que foi aprendido ao portar a UI do
[claude-usage-stick-SVGL](../../SRC/claude-usage-stick-SVGL-main) (LVGL 9, display
QSPI) para este hardware (LVGL 9, display i80/ST7796). Leia isto antes de alterar
qualquer coisa relacionada a display, touch ou inicialização do LVGL.

## Identificação

| Item | Valor |
|---|---|
| Placa | Smart Panlee / Wireless-Tag SC01 Plus `ZX3D50CE08S-V16-USRC`, PCB `240221` |
| MCU | ESP32-S3 QFN56, revisão 0.2 |
| Flash | 16 MiB, DIO, 80 MHz |
| PSRAM | 2 MiB físicos, Quad (não OPI) — **não é usada pelo buffer do LVGL** (ver abaixo) |
| Display | ST7796UI, 320×480 físico, interface i80/MCU 8080 de 8 bits |
| Touch | FT6336U/FT5x06, I²C, endereço `0x38` |
| Cartão | microSD SPI, FAT32 |
| ESP-IDF | 5.5.x |
| Orientação usada por este firmware | **Paisagem, 480×320 lógico** |

## Pinagem

### LCD ST7796UI (i80/8080)

| Sinal | GPIO |
|---|---:|
| D0…D7 | 9, 46, 3, 8, 18, 17, 16, 15 |
| DC | 0 |
| WR | 47 |
| RST | 4 |
| Backlight (PWM/LEDC) | 45 |
| TE | 48 |
| CS / RD | não usados (`-1`) |

### Touch FT6336U

| Sinal | GPIO |
|---|---:|
| SDA | 6 |
| SCL | 5 |
| INT | 7 |
| RST | 4, compartilhado com o LCD — **não controlar separadamente** |

I²C a 100 kHz, endereço `0x38`.

### microSD (SPI)

| Sinal | GPIO |
|---|---:|
| CLK | 39 |
| MOSI | 40 |
| MISO | 38 |
| CS | 41 |

Todos os valores acima estão em `main/board.h`; foram herdados do bring-up validado
em hardware real (29/07/2026) e **não devem ser alterados sem um build de teste
isolado e nova validação visual/serial**.

## Contrato de cor do LCD (não alterar sem teste isolado)

Esta é a parte mais importante para evitar cores trocadas ou imagens degradadas —
regra herdada do bring-up original e mantida à risca neste firmware
(`main/bsp_display.c`):

```c
io_config.flags.swap_color_bytes = 1;   // barramento i80

const esp_lcd_panel_dev_config_t panel_config = {
    .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
    .bits_per_pixel = 16,
};

esp_lcd_panel_invert_color(panel, true);   // inversão do controlador, sempre habilitada
```

- Envie RGB565 canônico, **sem conversão manual G↔B** em nenhum caminho (LVGL ou
  qualquer bitmap futuro).
- `bsp_display_draw_bitmap_raw()` é o único caminho de envio de pixels ao painel —
  tanto o `flush_cb` do LVGL quanto qualquer rotina futura de imagem devem passar por
  ele sem transformação de canal.
- Não compense a cor do tema padrão do LVGL (os botões saem azuis por padrão com essa
  configuração — isso é o comportamento correto).

**Regra prática:** não altere byte swap, ordem RGB/BGR, inversão, espelhamento ou
`swap_xy` ao mesmo tempo. Cada mudança exige um build isolado, com uma única
diferença, validado visualmente contra uma imagem de referência (faixas de cor +
escala de cinza) antes do próximo teste.

## Orientação: paisagem (480×320)

Este firmware roda **sempre em paisagem** (`BSP_LCD_LANDSCAPE 1` em `board.h`), pois é
o layout para o qual a UI do Claude Usage Stick foi desenhada (tileview horizontal,
cards lado a lado). Configuração do painel:

```c
esp_lcd_panel_mirror(panel, false, false);
esp_lcd_panel_swap_xy(panel, true);
```

Mapeamento de touch validado para esta orientação (mesma fórmula do bring-up
original, cinco alvos validados fisicamente):

```c
logical_x = raw_y;
logical_y = 319 - raw_x;
```

Não aplique calibração adicional sem nova medição em hardware.

## Inicialização do LVGL 9 nesta placa (diferente do projeto original!)

O firmware original (`claude-usage-stick-SVGL-main`, display QSPI AXS15231B) usa
**buffer de tela cheia (full-frame) em PSRAM** porque seu barramento QSPI escreve para
um framebuffer em RAM antes de mandar para o painel:

```c
// firmware ORIGINAL (AXS15231B/QSPI) — NÃO é o padrão usado nesta placa
uint32_t bufSize = SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(lv_color_t);  // ~300 KB
lv_color_t *buf = heap_caps_malloc(bufSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
lv_display_set_buffers(disp, buf, NULL, bufSize, LV_DISPLAY_RENDER_MODE_FULL);
```

Isso **não se aplica aqui**. O painel ST7796 desta placa tem GRAM própria e o
barramento i80 já transfere por DMA em blocos linha a linha
(`bsp_display_draw_bitmap_raw()`); o bring-up desta placa validou um **buffer
parcial de 20 linhas em RAM interna**, e é isso que `main/app_main.c` usa:

```c
const size_t draw_pixels = BSP_LCD_H_RES * BSP_LCD_DRAW_LINES;   // 480 * 20
lv_color_t *draw_buf = heap_caps_malloc(draw_pixels * sizeof(lv_color_t),
                                        MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
lv_display_t *display = lv_display_create(BSP_LCD_H_RES, BSP_LCD_V_RES);
lv_display_set_flush_cb(display, lvgl_flush_cb);
lv_display_set_buffers(display, draw_buf, NULL, draw_pixels * sizeof(lv_color_t),
                        LV_DISPLAY_RENDER_MODE_PARTIAL);
```

**Não copie o padrão `LV_DISPLAY_RENDER_MODE_FULL` + PSRAM do projeto original para
esta placa** — não é necessário (o painel já tem memória própria) e a PSRAM Quad de
2 MiB desta placa nunca foi validada para esse uso. A PSRAM continua disponível via
`heap_caps_malloc(..., MALLOC_CAP_SPIRAM)` para outros usos (ex.: buffers maiores de
JSON), mas não é usada pelo pipeline gráfico.

Resto da inicialização, igual em espírito ao original — só a API mudou de
Arduino/`lv_disp_drv_t` (LVGL 8-style) para o C puro de `lv_display_t` (LVGL 9):

```c
lv_init();
lv_tick_set_cb(lv_tick_cb);              // uint32_t lv_tick_cb(void) -> ms desde o boot
lv_indev_t *touch_indev = lv_indev_create();
lv_indev_set_type(touch_indev, LV_INDEV_TYPE_POINTER);
lv_indev_set_read_cb(touch_indev, lvgl_touch_read_cb);
```

O callback de flush chama `lv_disp_flush_ready(disp)` ao final — esse nome (não
`lv_display_flush_ready`) é o confirmado como funcional no firmware original sob
LVGL 9.2.2 e foi mantido aqui por segurança.

## Por que LVGL 9 (e não 8.3.11) nesta placa

O pacote inicial da SC01 Plus veio com LVGL 8.3.11 fixado. Este firmware **sobe para
LVGL 9.2.2** (mesma versão do projeto original) porque a UI inteira do Claude Usage
Stick já foi escrita contra a API do LVGL 9 (`lv_display_create`, `lv_image_create`,
`lv_buttonmatrix`, medidores/gráficos desenhados à mão com `lv_obj`/`lv_line`, nunca
`lv_meter`/`lv_chart`/`lv_canvas`). Isso permitiu reaproveitar ~90% do código de UI
quase literalmente — só a camada de driver de display/touch (acima) precisou ser
reescrita, o que já seria necessário de qualquer forma porque o painel i80/ST7796
nada tem a ver com o QSPI/AXS15231B original. O contrato de cor e o mapeamento de
touch descritos acima são **independentes da versão do LVGL** — continuam os mesmos.

**Duas armadilhas confirmadas em build real** (esta é a parte mais propensa a
quebrar silenciosamente entre versões do componente `lvgl/lvgl` — confira aqui
primeiro se aparecer `error: 'lv_font_montserrat_NN' undeclared` ou
`fatal error: lv_conf.h: No such file or directory`):

1. **`CONFIG_LV_CONF_SKIP` faz o contrário do que o nome sugere.** Ele manda
   `lv_conf_internal.h` **pular** o `#include "lv_conf.h"` do projeto e usar só
   os defaults gerados via Kconfig (que habilitam só a fonte `montserrat_14`,
   por exemplo — daí o erro `lv_font_montserrat_48 undeclared`). Pior: o
   componente define o **default do próprio Kconfig como `y`** (habilitado),
   então só *omitir* a linha em `sdkconfig.defaults` não basta — é preciso
   desabilitar explicitamente com a sintaxe do Kconfig para isso:
   ```
   # CONFIG_LV_CONF_SKIP is not set
   ```

2. **Mesmo com o skip desabilitado, os arquivos internos do próprio LVGL não
   enxergam `main/lv_conf.h`.** O `env_support/cmake/esp.cmake` do componente
   registra `INCLUDE_DIRS ${LVGL_ROOT_DIR}/../` — ou seja, só
   `managed_components/` (o pai da pasta `lvgl__lvgl`), nunca `main/`. Então
   `main/*.c` acha `lv_conf.h` (porque tem `-Imain`), mas `lv_group.c`,
   `lv_obj.c` etc. (compilados dentro do próprio componente `lvgl__lvgl`) não
   têm `-Imain` e falham com `fatal error: lv_conf.h: No such file or
   directory`. **Correção definitiva:** definir `LV_CONF_PATH` (mecanismo
   documentado no próprio `lv_conf_internal.h`, tem prioridade sobre
   `LV_CONF_INCLUDE_SIMPLE` e não depende de include path) como propriedade de
   build **global** — feito em `main/CMakeLists.txt`, antes de
   `idf_component_register`:
   ```cmake
   if(NOT CMAKE_BUILD_EARLY_EXPANSION)
       idf_build_set_property(COMPILE_DEFINITIONS
           "LV_CONF_PATH=\"${CMAKE_CURRENT_LIST_DIR}/lv_conf.h\"" APPEND)
   endif()
   ```
   O guard `CMAKE_BUILD_EARLY_EXPANSION` é necessário porque `CMakeLists.txt`
   de componente roda duas vezes: uma passada leve, só para descobrir
   `REQUIRES`/`PRIV_REQUIRES` (onde `idf_build_set_property` ainda não existe
   como comando), e a passada completa depois. `idf_build_set_property(COMPILE_DEFINITIONS ...)`
   se aplica a **todos** os componentes do build, não só a `main` — por isso
   funciona mesmo para arquivos internos do LVGL. Com isso, `CONFIG_LV_CONF_SKIP is not set` deixa
   de ser estritamente necessário (o `LV_CONF_PATH` força a inclusão de
   qualquer forma), mas mantenha os dois: são baratos e tornam a intenção
   explícita.

Se isso mudar de novo em versões futuras do componente `lvgl/lvgl`, confira
`src/lv_conf_internal.h` e `env_support/cmake/esp.cmake` dele antes de mexer
nessas duas configurações.

## Assets de imagem (mascotes Clawd + logotipo)

`main/logo_assets.h` é gerado por `tools/gen_logo_assets.py` a partir dos SVGs
oficiais em `assets/brand/`. O formato já é `lv_image_dsc_t` / `LV_COLOR_FORMAT_ARGB8888`
— exatamente o esperado pelo LVGL 9 — então o arquivo é copiado **sem nenhuma
alteração** do projeto original. Não regenerar a menos que os SVGs de origem mudem.

## microSD: histórico e log

- Formato: FAT32, montagem somente leitura/gravação simples via `esp_vfs_fat_sdspi_mount`
  (`main/bsp_sdcard.c`).
- `HISTORY.DAT` (nome 8.3, `main/config.h::HISTORY_FILE_PATH`) guarda o ring buffer de
  amostras da janela de 5h (160 amostras, ver `main/storage.c`) e o heatmap por dia (31
  dias). É reescrito por inteiro a cada refresh bem-sucedido — não há escrita
  incremental.
- Se o cartão não montar, o firmware **continua funcionando normalmente**: só o
  histórico/tendência/heatmap ficam vazios até haver um cartão. Isso é diferente do
  projeto original, que persistia em LittleFS interno (decisão deliberada para esta
  placa: usar o slot SD já validado em vez de uma nova partição de dados).
- Log persistente de depuração: `/sdcard/CLAUDESK.LOG` (nome 8.3), via
  `main/bsp_debug_log.c` — mesmo padrão dos bring-ups anteriores desta placa
  (`SC0532.LOG`, `SC0535.LOG`).

## Armadilhas conhecidas (gotchas)

- **Fontes Montserrat embutidas são ASCII + alguns símbolos apenas** (°, •, os ícones
  `LV_SYMBOL_*`). Textos com acentuação usam UTF-8 (`\xE2\x80\xA2` = •, já usado no
  código); não assuma que qualquer caractere Unicode vai renderizar.
- **Fuso horário não usa `TZ` de processo.** O sistema fica sempre em UTC via SNTP
  (`time_sync_start()`); o deslocamento de fuso configurado pelo usuário (GMT±N) é
  somado manualmente só na hora de formatar (`fmt_clock`/`fmt_hm` em `ui_common.c`,
  `accumulate_heat`/`day_key` em `app_state.c`). Se adicionar um novo lugar que exiba
  hora local, replique esse padrão — não chame `localtime_r` esperando o offset
  configurado, ele sempre devolve UTC.
- **Animações (`lv_anim`) e overlays em `lv_layer_top()`** devem ser destruídos
  explicitamente (`moment_close()` chama `lv_obj_delete` no scrim) — um `lv_anim_t`
  apontando para um objeto já destruído derruba o firmware.
- **A busca de rede (`api.c`/`status.c`) roda numa task FreeRTOS dedicada**
  (`refresh_task_fn` em `app_main.c`), não na task do LVGL — diferente do firmware
  original (Arduino, um único `loop()` que bloqueava ~1-2 s a cada refresh). Qualquer
  novo código que precise atualizar um `lv_obj_t*` a partir de dados vindos da rede
  deve seguir o mesmo padrão: a task de rede só escreve em variáveis simples
  (`g_usage`, `g_status`, flags), e a aplicação na tela acontece de volta na task
  principal (loop de `app_main`), nunca dentro da task de rede.
- **`esp_http_server` e o callback de `/token`** também rodam fora da task do LVGL —
  por isso `onboarding_on_token_submitted()` (`ui_onboarding.c`) só mexe em estado
  puro; quem atualiza o label na tela é `ui_token_tick()`, chamado a cada iteração do
  loop principal.
- **Qualquer task que faça uma chamada HTTPS/TLS (`esp_http_client`) precisa de pilha
  generosa (>= 8 KiB) — confirmado em hardware duas vezes.** Primeiro na task
  `"main"` (`app_main()` fazendo display+Wi-Fi+mDNS+HTTP server+SD inline estourava o
  default de 3584 bytes; corrigido com `CONFIG_ESP_MAIN_TASK_STACK_SIZE=12288` em
  `sdkconfig.defaults`). Depois na task do `esp_http_server`: o handler de
  `POST /token` chama `fetchUsage()` (uma requisição HTTPS completa) direto na task
  do httpd, cujo `stack_size` default (`HTTPD_DEFAULT_CONFIG()`, 4096 bytes) também
  estourava — corrigido setando `config.stack_size = 8192` antes de `httpd_start()`
  em `onboarding_server_start()`. Em ambos os casos o sintoma no serial foi um reset
  com `Saved PC: _WindowOverflow8` (assinatura clássica de stack overflow no Xtensa,
  não confundir com o "Saved PC" inofensivo que aparece em qualquer reset externo via
  RTS do `esptool` — se o símbolo decodificado for `_WindowOverflow8` ou aparecer o
  banner `***ERROR*** A stack overflow in task ... has been detected`, é real). Se
  criar uma nova task ou handler que chame `api.c`/`status.c`, dimensione a pilha
  com a mesma margem.
- **PSRAM Quad (não OPI)**: presença física confirmada por `esptool`, mas o
  inicializador é herdado do `sdkconfig` legado da família ZX3D50CE02/08 — validar em
  hardware antes de depender dela para qualquer buffer grande novo.

## Procedimento recomendado ao alterar algo hardware-sensível

1. Copie primeiro a pinagem e o contrato de cor deste documento — não adivinhe.
2. Não misture parâmetros de retrato e paisagem.
3. Teste o padrão de cores (ver `docs/REFERENCIA_PLACA_SC01_PLUS.md` para o BMP de
   referência) antes de mexer em qualquer novo caminho de imagem.
4. Teste touch com alvos grandes antes de aplicar qualquer calibração nova.
5. Ative SD, Wi-Fi e a task de refresh separadamente ao depurar um problema novo.
6. A cada build: aumente `FW_VERSION` (`main/config.h`), registre a alteração isolada
   e a evidência visual/serial em `docs/REGISTRO_DE_BUILDS.md`.

## Evidências de origem

- Contrato de cor e touch em paisagem: bring-up da SC01 Plus, builds
  `0.5.27`–`0.5.32-landscape-unmirror`, ver `docs/REFERENCIA_PLACA_SC01_PLUS.md` e
  `docs/DICIONARIO_CONFIGURACAO_PLACA.md` (pacote inicial da placa).
- UI, mascotes, medidores, gráfico de tendência e heatmap: porte de
  `claude-usage-stick-SVGL-main/firmware/claude_stick/claude_stick.ino`.
- Histórico por build deste firmware: `docs/REGISTRO_DE_BUILDS.md`.
