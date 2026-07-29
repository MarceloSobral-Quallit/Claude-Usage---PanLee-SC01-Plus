# Referência reutilizável — SC01 Plus ZX3D50CE08S-V16-USRC

Documento de referência da placa, adaptado do pacote inicial (`SC01_PLUS_STARTER`)
para o firmware Claude Usage Stick. Os parâmetros abaixo foram confirmados em
hardware nesta unidade em 29/07/2026. Para o que mudou especificamente por causa
deste firmware (LVGL 9, buffer parcial, histórico no SD), ver
[`firmware/panlee_sc01_plus/REFERENCIA-HARDWARE-LVGL.md`](../firmware/panlee_sc01_plus/REFERENCIA-HARDWARE-LVGL.md).

## Identificação

| Item | Valor confirmado |
|---|---|
| Placa | Smart Panlee / Wireless-Tag SC01 Plus `ZX3D50CE08S-V16-USRC` |
| PCB | `240221` |
| MCU | ESP32-S3 QFN56, revisão 0.2 |
| Flash | 16 MiB, DIO, 80 MHz |
| PSRAM | 2 MiB físicos |
| Display | ST7796UI, 320×480, i80/MCU 8080 de 8 bits |
| Touch | FT6336U/FT5x06, I²C, endereço `0x38` |
| Cartão | microSD SPI, FAT32 validado |
| ESP-IDF usado | 5.5.x |

## Pinagem

### LCD ST7796UI

| Sinal | GPIO |
|---|---:|
| D0…D7 | 9, 46, 3, 8, 18, 17, 16, 15 |
| DC | 0 |
| WR | 47 |
| RST | 4 |
| Backlight | 45 |
| TE | 48 |
| CS / RD | não usados |

### Touch

| Sinal | GPIO |
|---|---:|
| SDA | 6 |
| SCL | 5 |
| INT | 7 |
| RST | 4, compartilhado com o LCD — não controlar separadamente |

### microSD SPI

| Sinal | GPIO |
|---|---:|
| CLK | 39 |
| MOSI | 40 |
| MISO | 38 |
| CS | 41 |

## Contrato de cor do LCD

Esta é a parte mais importante para evitar cores trocadas ou imagens degradadas.

```c
io_config.flags.swap_color_bytes = 1;

const esp_lcd_panel_dev_config_t panel_config = {
    .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
    .bits_per_pixel = 16,
};
```

- Envie RGB565 canônico, sem conversão manual G↔B.
- Tanto imagens BMP quanto o `flush` do LVGL devem usar o caminho de envio *raw*.
- Não compense o azul dos botões no tema LVGL: o tema padrão já fica azul com a configuração acima.
- A inversão do painel deve permanecer habilitada:

```c
esp_lcd_panel_invert_color(panel, true);
```

### Verificação rápida de cor

Use uma imagem com faixas vermelho, verde, azul, ciano, magenta, amarelo, branco e
preto (`COLOR_TEST_320x480.bmp` do pacote inicial serve de referência). A referência
validada apresenta as faixas nessa ordem, cinza contínuo e gradiente sem inversões.

## Orientação retrato

Configuração aprovada para interface 320×480:

```c
esp_lcd_panel_mirror(panel, true, false);
/* Não chamar esp_lcd_panel_swap_xy(). */
```

Touch em retrato: usar diretamente `(x, y)` recebidos do controlador. Não há calibração geométrica adicional.

## Orientação paisagem (usada por este firmware)

Configuração aprovada para interface 480×320:

```c
esp_lcd_panel_mirror(panel, false, false);
esp_lcd_panel_swap_xy(panel, true);
```

Transformação de touch validada pelos cinco alvos (quatro cantos e centro):

```c
logical_x = raw_y;
logical_y = 319 - raw_x;
```

Não aplique calibração adicional sem nova medição.

## LVGL

- Este firmware usa **LVGL 9.2.2** (ver
  [`firmware/panlee_sc01_plus/REFERENCIA-HARDWARE-LVGL.md`](../firmware/panlee_sc01_plus/REFERENCIA-HARDWARE-LVGL.md)
  para a justificativa e as diferenças de inicialização).
- Buffer parcial: 20 linhas, em RAM interna.
- O callback de flush deve encaminhar o `color_map`/`px_map` sem conversão de canais,
  via `bsp_display_draw_bitmap_raw`.

## microSD e logs

- Formato validado: FAT32.
- Não formatar automaticamente o cartão em caso de falha de montagem.
- Para compatibilidade FAT 8.3, use nomes de log/dados como `CLAUDESK.LOG` e
  `HISTORY.DAT`.
- O firmware registra log persistente no SD (quando presente) e também no serial.

## Wi-Fi e onboarding

Modo validado: cliente/STA, com onboarding 100% pelo dispositivo (scan + teclado na
tela, até 3 redes salvas em NVS) — ver README na raiz do repositório. `wifi_cred.txt`
na raiz (fora do controle de versão) só serve como conveniência opcional de
desenvolvimento: se presente, pré-semeia uma rede na primeira inicialização.

## Ambiente e comandos

```powershell
. $env:IDF_PATH\export.ps1      # ou o atalho "ESP-IDF 5.5 PowerShell" instalado pelo instalador oficial
cd firmware\panlee_sc01_plus
idf.py set-target esp32s3
idf.py build
idf.py -p COM3 flash monitor
```

> O instalador oficial do ESP-IDF já cria seu próprio ambiente Python virtual
> (`~/.espressif`) para as ferramentas — **nunca** `pip install` os requisitos do
> ESP-IDF no Python global da máquina. Ver seção "Ambiente de desenvolvimento" no
> README para o mesmo cuidado com as ferramentas em `tools/` (Python).

## Procedimento recomendado ao iniciar um novo teste

1. Copiar primeiro a pinagem e o contrato de cor deste documento.
2. Escolher uma orientação; não misturar parâmetros de retrato e paisagem.
3. Testar o padrão de cores antes de implementar qualquer novo caminho de imagem.
4. Testar touch com alvos grandes antes de aplicar qualquer calibração.
5. Ativar SD, Wi-Fi e a task de refresh separadamente.
6. A cada build, registrar versão, alteração isolada, hash da gravação e evidência
   visual/serial em `docs/REGISTRO_DE_BUILDS.md`.

## Limites e cuidados

- A PSRAM física é Quad (não OPI) e sua inicialização vem de um `sdkconfig` legado da
  família ZX3D50CE02/08; validar em hardware antes de adotá-la para novos usos além do
  que este firmware já faz (ele não depende dela para o pipeline gráfico).
- Uma captura de tela física não é possível pelo barramento i80 de escrita atual sem
  framebuffer próprio ou suporte de leitura adicional.
- RS485, áudio, mouse BLE e slideshow de JPEG não fazem parte deste firmware (fora de
  escopo do Claude Usage Stick).

## Evidências de origem

- Retrato com cores e LVGL aprovados: builds `0.5.27` e `0.5.28` (pacote inicial da placa).
- Paisagem e touch aprovados: build `0.5.32-landscape-unmirror`, log `SC0532.LOG`.
- Porte da UI para este firmware: ver
  [`firmware/panlee_sc01_plus/REFERENCIA-HARDWARE-LVGL.md`](../firmware/panlee_sc01_plus/REFERENCIA-HARDWARE-LVGL.md).
- Histórico detalhado: `docs/REGISTRO_DE_BUILDS.md`.
