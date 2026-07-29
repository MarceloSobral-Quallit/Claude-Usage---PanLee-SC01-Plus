<#
.SYNOPSIS
    Ativa, na sessao ATUAL do PowerShell, o ambiente ESP-IDF local deste repo
    (criado por tools/setup_esp_idf_env.ps1).

.DESCRIPTION
    Precisa ser executado com "dot-sourcing" (o ponto no inicio), senao as
    variaveis de ambiente (PATH, IDF_PATH, IDF_TOOLS_PATH) somem ao terminar
    o script e o "idf.py" nao fica disponivel no seu terminal:

        . .\tools\activate-esp-idf.ps1

    Nao altera nada fora desta sessao do PowerShell nem fora deste repositorio.
#>

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$EnvRoot = Join-Path $RepoRoot ".idf-env"
$IdfDir = Join-Path $EnvRoot "esp-idf"
$ToolsDir = Join-Path $EnvRoot "tools"

if (-not (Test-Path $IdfDir)) {
    Write-Error "ESP-IDF nao encontrado em $IdfDir. Rode primeiro: .\tools\setup_esp_idf_env.ps1"
    return
}

$env:IDF_TOOLS_PATH = $ToolsDir
. (Join-Path $IdfDir "export.ps1")
