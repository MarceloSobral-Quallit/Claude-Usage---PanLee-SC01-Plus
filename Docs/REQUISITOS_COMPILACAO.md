# Requisitos de compilação — ambiente local ao repositório

Este projeto mantém o ambiente de build (checkout do ESP-IDF + toolchain + venv
Python) **dentro do próprio repositório**, em `.idf-env/` (fora do controle de
versão — ver `.gitignore`), em vez de instalar globalmente em `~/esp` ou
`~/.espressif` como o instalador oficial faz por padrão. Isso significa:

- Nada é instalado no Python global da máquina nem em pastas fora deste repositório.
- Cada checkout deste repositório tem sua própria versão do ESP-IDF/toolchain,
  sem conflitar com outros projetos ESP-IDF que você tenha na mesma máquina.
- O único custo é espaço em disco: se você já tem ESP-IDF instalado globalmente,
  isso baixa uma cópia adicional (não compartilha).

## O que é versionado vs. o que é baixado

| Versionado no git | Baixado por `tools/setup_esp_idf_env.ps1` (gitignored) |
|---|---|
| `tools/setup_esp_idf_env.ps1` (script de instalação) | `.idf-env/esp-idf/` — checkout do framework ESP-IDF |
| `tools/activate-esp-idf.ps1` (script de ativação) | `.idf-env/tools/` — toolchain (GCC Xtensa) + venv Python próprio do ESP-IDF |
| Este documento | |

**Nenhum binário de toolchain é commitado no git.** Commitar o ESP-IDF e o
toolchain (vários GB, específicos de cada sistema operacional) infla o
repositório sem necessidade — o padrão do ecossistema (equivalente a
`node_modules/` ou `.venv/`) é versionar só o script que reproduz o ambiente,
não o ambiente em si. O que fica local ao repo (em vez de global na máquina) é
**onde** esses downloads são colocados, não o fato de existirem.

## Requisitos

- Windows 10/11 com PowerShell 7+ (ou o Windows PowerShell 5.1 padrão).
- Git (já usado pelo próprio repositório).
- Python 3.9–3.13 já instalado no sistema (usado só para criar o venv do
  ESP-IDF; nenhuma dependência é instalada nele diretamente).
- ~6–8 GiB livres em disco (checkout do ESP-IDF + submódulos + toolchain).
- Conexão à internet para a primeira instalação (`git clone` + download do
  toolchain); depois disso, compilar não precisa mais de rede.

## Uso

```powershell
# Uma vez (ou quando quiser recriar o ambiente):
.\tools\setup_esp_idf_env.ps1

# Em toda sessão nova do PowerShell em que for compilar:
. .\tools\activate-esp-idf.ps1

cd firmware\panlee_sc01_plus
idf.py set-target esp32s3
idf.py build
idf.py -p COM3 flash monitor
```

`setup_esp_idf_env.ps1` é reentrante: se `.idf-env/esp-idf` já existir, ele pula o
clone (apague a pasta manualmente para reclonar do zero ou trocar de versão).

## Versão do ESP-IDF usada

`tools/setup_esp_idf_env.ps1` clona a branch `release/v5.5` por padrão — a mesma
linha (5.5.x) usada na validação de hardware desta placa
(`docs/REFERENCIA_PLACA_SC01_PLUS.md` registra `5.5.5` como a versão exata testada
fisicamente). Para fixar uma versão diferente:

```powershell
.\tools\setup_esp_idf_env.ps1 -IdfRef v5.5.5
```

## Por que esta abordagem (e quando ela NÃO é a certa)

Válida para este projeto porque:

- É um único firmware, então não há ganho em compartilhar um ESP-IDF global
  entre "vários projetos" na mesma máquina.
- O pedido explícito do projeto é evitar qualquer alteração no ambiente do
  desenvolvedor fora do repositório (venv sempre, nunca instalação global).
- Reprodutibilidade: qualquer pessoa que clonar o repositório e rodar um único
  script reproduz exatamente o ambiente esperado, sem depender do que já está
  (ou não) instalado globalmente na máquina dela.

Ressalva: se você já tem um ESP-IDF 5.5.x global configurado (ex.: o atalho
"ESP-IDF 5.5 PowerShell" do instalador oficial) e não se importa em compartilhá-lo
entre projetos, pode simplesmente usá-lo em vez deste script — os dois convivem
sem conflito, já que `activate-esp-idf.ps1` só afeta a sessão atual do terminal.
