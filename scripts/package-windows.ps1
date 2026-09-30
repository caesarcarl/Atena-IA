param(
    [string]$QtVersion = "6.8.3",
    [switch]$InstallSystemTools,
    [switch]$Clean
)
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
& "$PSScriptRoot\build-windows.ps1" -Configuration Release -QtVersion $QtVersion -InstallSystemTools:$InstallSystemTools -Clean:$Clean

$Deps = Join-Path $Root ".deps"
$State = Get-Content (Join-Path $Deps "windows-env.json") -Raw | ConvertFrom-Json
$QtRoot = [string]$State.ATENA_QT_ROOT
$Build = Join-Path $Root "build-windows"
$StageRoot = Join-Path $Root "dist"
$Stage = Join-Path $StageRoot "Atena-0.4.3-Windows-x64"
Remove-Item $Stage -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $Stage | Out-Null

& cmake --install $Build --config Release --prefix $Stage
if ($LASTEXITCODE -ne 0) { throw "cmake --install falhou." }

$Bin = Join-Path $Stage "bin"
$Ui = Join-Path $Bin "atena-ui.exe"
$Core = Join-Path $Bin "atena-core.exe"
$Cli = Join-Path $Bin "atena.exe"
foreach ($f in @($Ui,$Core,$Cli)) { if (-not (Test-Path $f)) { throw "Instalação incompleta: $f" } }
foreach ($f in @("persona.json","pedagogy.json","regional.json")) {
    if (-not (Test-Path (Join-Path $Bin "identity\$f"))) { throw "Identity incompleta: $f" }
}

$Windeploy = Join-Path $QtRoot "bin\windeployqt.exe"
& $Windeploy --release --no-translations --compiler-runtime --dir $Bin $Ui
if ($LASTEXITCODE -ne 0) { throw "windeployqt falhou." }

$VcpkgBin = Join-Path $Deps "vcpkg_installed\x64-windows\bin"
if (Test-Path $VcpkgBin) {
    Get-ChildItem $VcpkgBin -Filter *.dll | Copy-Item -Destination $Bin -Force
}

# Validate that the CLI can at least load all runtime DLLs without starting the Core.
& $Cli --help | Out-Host
if ($LASTEXITCODE -ne 0) { throw "Smoke test do atena.exe falhou. Verifique DLLs do pacote." }

$Readme = @"
Atena IA 0.4.3 para Windows x64
================================

Executáveis:
  bin\atena-ui.exe   interface Qt 6
  bin\atena-core.exe Core C17
  bin\atena.exe      CLI

O Core é iniciado automaticamente pela UI/CLI através de Named Pipes.
Dados do usuário: %LOCALAPPDATA%\Atena
Configuração:     %APPDATA%\Atena
Chaves de API:    Windows Credential Manager
Log do Core:      %LOCALAPPDATA%\Atena\core.log

Providers desta revisão:
  - Ollama local/remoto
  - OpenAI
  - Groq
  - DeepSeek
  - xAI
  - OpenAI-compatible

Anthropic e Gemini nativos ainda não estão implementados.
"@
$Readme | Set-Content -Encoding UTF8 (Join-Path $Stage "LEIA-ME-WINDOWS.txt")

& "$PSScriptRoot\verify-windows.ps1" -Stage $Stage

$Portable = Join-Path $StageRoot "Atena-0.4.3-Windows-x64-Portable.zip"
Remove-Item $Portable -Force -ErrorAction SilentlyContinue
Compress-Archive -Path "$Stage\*" -DestinationPath $Portable -CompressionLevel Optimal

$Makensis = Get-Command makensis.exe -ErrorAction SilentlyContinue
if (-not $Makensis) {
    $nsisCandidates = @(
        "$env:ProgramFiles\NSIS\makensis.exe",
        "$env:ProgramFiles(x86)\NSIS\makensis.exe"
    )
    foreach ($candidate in $nsisCandidates) {
        if (Test-Path $candidate) { $Makensis = Get-Item $candidate; break }
    }
}
if ($Makensis) {
    & $Makensis.Source "/DROOT=$Root" "/DSTAGE=$Stage" "$Root\packaging\windows\Atena.nsi"
    if ($LASTEXITCODE -ne 0) { throw "NSIS falhou ao gerar instalador." }
    Write-Host "Instalador: $Root\Atena-Setup-0.4.3-x64.exe" -ForegroundColor Green
} else {
    Write-Warning "NSIS não encontrado. ZIP portátil gerado; instale NSIS e execute novamente para obter o setup .exe."
}
Write-Host "Portátil: $Portable" -ForegroundColor Green
