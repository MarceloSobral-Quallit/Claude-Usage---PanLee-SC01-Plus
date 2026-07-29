# Índice da documentação — Claude Usage Stick (Panlee SC01 Plus)

Ponto de entrada único para toda a documentação do projeto. Ver também o
[`README.md`](../README.md) da raiz (visão de produto, telas, build/flash) e
[`firmware/panlee_sc01_plus/REFERENCIA-HARDWARE-LVGL.md`](../firmware/panlee_sc01_plus/REFERENCIA-HARDWARE-LVGL.md)
(referência técnica principal de hardware/LVGL para esta placa).

## Documentos de projeto (`docs/`)

| Documento | Resumo |
|---|---|
| [`README.md`](README.md) | Escopo funcional e estado atual do projeto — o que está entregue/validado em hardware e o que ainda falta. |
| [`DEV_PLAYBOOK.md`](DEV_PLAYBOOK.md) | Decisões técnicas (por quê LVGL 9, SD vs. partição interna, ambiente local ao repo, lições de stack overflow) e o roadmap de próximos passos. |
| [`protocolo-ciclico-publicacao-exclusao.md`](protocolo-ciclico-publicacao-exclusao.md) | Protocolo cíclico Contrato → Implementação → Execução técnica → Auditoria, adaptado para este firmware. |
| [`REQUISITOS_COMPILACAO.md`](REQUISITOS_COMPILACAO.md) | Ambiente de build local ao repositório (`.idf-env/`), requisitos, uso de `tools/setup_esp_idf_env.ps1` / `tools/activate-esp-idf.ps1`. |
| [`REFERENCIA_PLACA_SC01_PLUS.md`](REFERENCIA_PLACA_SC01_PLUS.md) | Referência de hardware da placa SC01 Plus (pinagem, contrato de cor, orientação) — base herdada do bring-up original. |
| [`DICIONARIO_CONFIGURACAO_PLACA.md`](DICIONARIO_CONFIGURACAO_PLACA.md) | Dicionário de parâmetros confirmados em hardware vs. pendentes de validação. |
| [`REGRAS_DESENVOLVIMENTO_LCD.md`](REGRAS_DESENVOLVIMENTO_LCD.md) | Regras de não regressão para o contrato de cor/LCD e para as tasks que fazem chamadas de rede. |
| [`REGISTRO_DE_BUILDS.md`](REGISTRO_DE_BUILDS.md) | Registro cronológico de builds gravados em hardware real, alteração testada e evidência. |

## Documentos fora de `docs/` mas centrais ao projeto

| Documento | Resumo |
|---|---|
| [`../README.md`](../README.md) | README de produto/publicação (GitHub): telas, hardware, build & flash, segurança, créditos. |
| [`../firmware/panlee_sc01_plus/REFERENCIA-HARDWARE-LVGL.md`](../firmware/panlee_sc01_plus/REFERENCIA-HARDWARE-LVGL.md) | Referência técnica única de display/touch/LVGL 9 para esta placa — inclui as armadilhas confirmadas em hardware (LV_CONF_SKIP/LV_CONF_PATH, stack overflow em tasks com TLS). Leia antes de mexer em display/touch/rede. |

## Como manter este índice atualizado

Sempre que um documento novo for adicionado em `docs/` (ou um existente for renomeado/movido),
atualizar a tabela acima na mesma rodada — é parte da rotina obrigatória do comando `/roadmap`
deste projeto (ver `protocolo-ciclico-publicacao-exclusao.md`).
