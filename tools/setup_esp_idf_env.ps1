<#
.SYNOPSIS
    Cria (ou atualiza) o ambiente ESP-IDF usado por este repositorio, inteiramente
    dentro dele — nada e instalado no Python global nem em pastas fora do repo.

.DESCRIPTION
    Clona o ESP-IDF (branch fixada abaixo) e instala o toolchain do ESP32-S3 dentro
    de ".idf-env/" na raiz do repositorio (fora do controle de versao — ver
    .gitignore). O instalador oficial do ESP-IDF ja cria seu proprio venv Python
    (nao mexe no `pip` global); aqui so redirecionamos ONDE esse venv e o
    toolchain ficam, via IDF_TOOLS_PATH, para dentro do repo.

    Ver docs/REQUISITOS_COMPILACAO.md para requisitos, espaco em disco e o que
    este script faz passo a passo.

.EXAMPLE
    .\tools\setup_esp_idf_env.ps1
    # depois, em cada nova sessao do PowerShell:
    . .\tools\activate-esp-idf.ps1
#>
param(
    [string]$IdfRef = "release/v5.5",
    [string]$Target = "esp32s3"
)

$ErrorActionPreference = 'Stop'

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$EnvRoot = Join-Path $RepoRoot ".idf-env"
$IdfDir = Join-Path $EnvRoot "esp-idf"
$ToolsDir = Join-Path $EnvRoot "tools"

Write-Host "Repositorio: $RepoRoot"
Write-Host "Ambiente ESP-IDF (local, fora do git): $EnvRoot"

if (-not (Test-Path $EnvRoot)) {
    New-Item -ItemType Directory -Path $EnvRoot | Out-Null
}

if (Test-Path $IdfDir) {
    Write-Host "ESP-IDF ja clonado em $IdfDir — pulando clone (apague a pasta para reclonar)."
} else {
    Write-Host "Clonando ESP-IDF ($IdfRef) em $IdfDir ..."
    git clone -b $IdfRef --recursive https://github.com/espressif/esp-idf.git $IdfDir
}

# Redireciona o download do toolchain/venv para dentro do repo em vez de
# ~/.espressif (default). Isso e so para ESTE processo/instalacao.
$env:IDF_TOOLS_PATH = $ToolsDir

Write-Host "Instalando toolchain para '$Target' em $ToolsDir (baixa alguns GB na primeira vez) ..."
& (Join-Path $IdfDir "install.ps1") $Target

Write-Host ""
Write-Host "Pronto. Para compilar o firmware, em CADA sessao nova do PowerShell:"
Write-Host "  . .\tools\activate-esp-idf.ps1"
Write-Host "  cd firmware\panlee_sc01_plus"
Write-Host "  idf.py set-target esp32s3"
Write-Host "  idf.py build"
