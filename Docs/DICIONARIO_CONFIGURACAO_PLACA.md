# Dicionário de configuração — SC01 Plus

**Placa:** `ZX3D50CE08S-V16-USRC` · PCB `240221`
**Atualizado em:** 29/07/2026 · **Estado:** firmware Claude Usage Stick completo (LCD, touch, Wi-Fi/HTTPS, cripto, microSD, LVGL 9)

Este documento é a referência única dos parâmetros de hardware e firmware da placa
para este projeto. Cada item indica se foi confirmado em hardware, lido no código de
referência ou ainda requer validação.

## Plataforma

| Item | Valor | Estado/evidência |
|---|---|---|
| MCU | ESP32-S3 QFN56, revisão 0.2 | Confirmado pelo `esptool` |
| Flash física | 16 MiB, DIO, 80 MHz | Confirmado pelo `esptool`; `sdkconfig.defaults` |
| PSRAM | 2 MiB, Quad | Presença física confirmada pelo `esptool`; não usada pelo pipeline gráfico deste firmware (ver `REFERENCIA-HARDWARE-LVGL.md`) |
| ESP-IDF | 5.5.x | Ambiente e builds atuais |

## LCD ST7796UI — confirmado

| Item | Valor |
|---|---|
| Interface | i80/MCU 8080, 8 bits |
| Geometria lógica (este firmware) | 480×320, paisagem |
| Dados D0…D7 | GPIO 9, 46, 3, 8, 18, 17, 16, 15 |
| Controle | DC GPIO0; WR GPIO47; RST GPIO4; BL GPIO45 (PWM/LEDC); TE GPIO48; CS/RD não usados |
| Ordem de cor efetiva | `rgb_ele_order=BGR`, `swap_color_bytes=1` no i80, envio RGB565 sem conversão manual G↔B |
| Inversão do controlador | habilitada |
| Espelhamento/swap | paisagem: `mirror(false,false)` + `swap_xy(true)` |

## Touch FT5x06/FT6336U — confirmado

| Item | Valor |
|---|---|
| Interface | I²C, endereço `0x38`, 100 kHz |
| Pinos | SDA GPIO6; SCL GPIO5; INT GPIO7 |
| Reset | GPIO4 compartilhado com reset do LCD; não controlar separadamente |
| Mapeamento (paisagem) | `x=raw_y`, `y=319-raw_x`; sem calibração adicional |

## Interface gráfica — confirmada

| Item | Valor |
|---|---|
| Biblioteca | LVGL 9.2.2 (ver justificativa em `REFERENCIA-HARDWARE-LVGL.md`) |
| Buffer de desenho | 480×20 pixels, RAM interna, `LV_DISPLAY_RENDER_MODE_PARTIAL` |
| Atualização | flush via `bsp_display_draw_bitmap_raw` (DMA i80), sem conversão de canal |
| Backlight | PWM via LEDC (brilho configurável em Ajustes) |

## microSD — usado para histórico e log

| Item | Valor | Estado |
|---|---|---|
| Interface | SPI | Confirmado |
| CLK / MOSI / MISO / CS | GPIO39 / GPIO40 / GPIO38 / GPIO41 | `main/board.h` |
| Uso | `HISTORY.DAT` (ring buffer 5h + heatmap 31 dias) e `CLAUDESK.LOG` (log de depuração) | Ausência do cartão é tolerada; dashboard funciona sem histórico |

## Rede / API — confirmado

| Item | Valor |
|---|---|
| Wi-Fi | STA, onboarding por toque (scan + teclado), até 3 redes salvas em NVS |
| TLS | `esp_crt_bundle_attach` (bundle de CAs do mbedTLS), sem certificado customizado |
| Onboarding web + bridge de tokens | `esp_http_server` + `mdns` (`claude-stick.local`), rotas `/`, `/token`, `/window`, `/tokens` |

## Fora do escopo deste firmware

- Mouse BLE HID, slideshow de JPEG, servidor de arquivos genérico (recursos de
  bring-up do pacote inicial da placa, não relacionados ao Claude Usage Stick).
- RS485
- Áudio

## Fontes no repositório

- Implementação atual: `firmware/panlee_sc01_plus/main/`.
- Referência de bring-up original desta placa (fora deste repo): pacote inicial
  `SC01_PLUS_STARTER`.
- Referência de UI/produto original (fora deste repo): `claude-usage-stick-SVGL-main`.
- Histórico por build: `docs/REGISTRO_DE_BUILDS.md`.
