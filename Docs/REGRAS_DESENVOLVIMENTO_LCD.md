# Regras de desenvolvimento — LCD e cores

**Base validada:** bring-up da SC01 Plus (LCD/touch/LVGL mínimo) + porte completo do
Claude Usage Stick para LVGL 9, validado visualmente em 29/07/2026.

## Configuração que não deve ser alterada sem novo teste isolado

| Camada | Configuração aprovada |
|---|---|
| Painel ST7796/i80 | `swap_color_bytes=1` |
| Painel ST7796 | `rgb_ele_order=LCD_RGB_ELEMENT_ORDER_BGR` |
| Painel | inversão habilitada; paisagem = `mirror(false,false)` + `swap_xy(true)` |
| LVGL | o `flush_cb` deve enviar por `bsp_display_draw_bitmap_raw()`, buffer parcial (20 linhas) em RAM interna |
| Conversão RGB565 G↔B | não usar em nenhum fluxo |

## Regras práticas

1. Não alterar simultaneamente byte swap, ordem RGB/BGR, inversão, espelhamento/swap_xy ou conversão RGB565.
2. Toda alteração em um desses parâmetros exige um build de teste próprio, com uma única diferença e validação visual antes do próximo teste.
3. Use uma imagem de referência com faixas RGB, cinza e gradiente para validar qualquer mudança de pipeline gráfico.
4. O azul dos botões é fornecido pelo tema padrão do LVGL. Não compensar a cor no estilo do botão para corrigir falhas do driver.
5. Qualquer caminho de imagem novo (BMP, JPEG, PNG) deve entregar RGB565 canônico ao mesmo fluxo `raw`; qualquer necessidade de conversão deve ser comprovada com o padrão de cores.
6. A cada firmware: aumentar `FW_VERSION` (`main/config.h`), registrar build/gravação/hash e resultado em `docs/REGISTRO_DE_BUILDS.md`.
7. Preservar a configuração validada como referência. Antes de experiências, registrar claramente o ponto de retorno (ex.: branch/commit).
8. Fuso horário: o relógio do sistema fica sempre em UTC (SNTP); o deslocamento configurado pelo usuário é aplicado manualmente na formatação (ver `REFERENCIA-HARDWARE-LVGL.md`, seção "armadilhas"). Não tente configurar `TZ` de processo esperando um comportamento diferente sem atualizar todos os pontos que somam o offset manualmente.
9. Chamadas de rede (`api.c`/`status.c`) rodam numa task FreeRTOS dedicada, nunca na task do LVGL. Qualquer atualização de tela a partir de dados de rede deve escrever em uma variável simples e deixar a task principal aplicar na UI (ver `REFERENCIA-HARDWARE-LVGL.md`).

## Critério de aceitação visual

- Faixas: vermelho, verde, azul, ciano, magenta, amarelo, branco e preto na ordem da imagem de referência.
- Escala de cinza contínua de preto a branco.
- Gradiente sem inversões de canais.
- Fundo do dashboard escuro (paleta do Claude Usage Stick), texto claro, acento coral — ver paleta em `main/app_state.h`.
