param(
    [string]$QtVersion = "6.8.3",
    [string]$QtArch = "win64_msvc2022_64",
    [switch]$InstallSystemTools
)
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$Deps = Join-Path $Root ".deps"
New-Item -ItemType Directory -Force -Path $Deps | Out-Null

function Have([string]$Name) { return [bool](Get-Command $Name -ErrorAction SilentlyContinue) }
function Refresh-Path {
    $machine = [Environment]::GetEnvironmentVariable("Path", "Machine")
    $user = [Environment]::GetEnvironmentVariable("Path", "User")
    $env:Path = "$machine;$user"
}
function Install-Winget([string]$Id, [string]$Override = "") {
    if (-not (Have "winget.exe")) { throw "winget não está disponível para instalar $Id automaticamente." }
    $args = @("install", "--id", $Id, "--exact", "--accept-package-agreements", "--accept-source-agreements")
    if ($Override) { $args += @("--override", $Override) }
    & winget @args
    if ($LASTEXITCODE -ne 0) { throw "Falha ao instalar $Id via winget." }
}

if ($InstallSystemTools) {
    if (-not (Have "git.exe")) { Install-Winget "Git.Git" }
    if (-not (Have "cmake.exe")) { Install-Winget "Kitware.CMake" }
    if (-not (Have "python.exe")) { Install-Winget "Python.Python.3.12" }

    $vswhere = "$env:ProgramFiles(x86)\Microsoft Visual Studio\Installer\vswhere.exe"
    $hasVc = $false
    if (Test-Path $vswhere) {
        $installation = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        $hasVc = -not [string]::IsNullOrWhiteSpace($installation)
    }
    if (-not $hasVc) {
        Install-Winget "Microsoft.VisualStudio.2022.BuildTools" "--wait --passive --norestart --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"
    }
    if (-not (Have "makensis.exe")) {
        try { Install-Winget "NSIS.NSIS" } catch { Write-Warning "NSIS não foi instalado. O ZIP portátil ainda poderá ser gerado." }
    }
    Refresh-Path
}

foreach ($tool in @("git.exe", "cmake.exe", "python.exe")) {
    if (-not (Have $tool)) { throw "$tool não encontrado. Execute este script com -InstallSystemTools ou instale o requisito." }
}

$vswhere = "$env:ProgramFiles(x86)\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) { throw "Visual Studio 2022 Build Tools não encontrado." }
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if ([string]::IsNullOrWhiteSpace($vs)) { throw "Instale a workload 'Desktop development with C++' do Visual Studio 2022 Build Tools." }

$VcpkgRoot = if ($env:VCPKG_ROOT) { $env:VCPKG_ROOT } else { Join-Path $Deps "vcpkg" }
if (-not (Test-Path (Join-Path $VcpkgRoot "scripts\buildsystems\vcpkg.cmake"))) {
    if (Test-Path $VcpkgRoot) { Remove-Item $VcpkgRoot -Recurse -Force }
    & git clone --depth 1 https://github.com/microsoft/vcpkg.git $VcpkgRoot
    if ($LASTEXITCODE -ne 0) { throw "Falha ao baixar vcpkg." }
    & (Join-Path $VcpkgRoot "bootstrap-vcpkg.bat") -disableMetrics
    if ($LASTEXITCODE -ne 0) { throw "Falha ao preparar vcpkg." }
}

$QtRoot = $env:ATENA_QT_ROOT
if (-not $QtRoot) {
    $QtBase = Join-Path $Deps "Qt"
    $QtRoot = Join-Path $QtBase "$QtVersion\msvc2022_64"
    if (-not (Test-Path (Join-Path $QtRoot "bin\windeployqt.exe"))) {
        & python -m pip install --user --upgrade aqtinstall
        if ($LASTEXITCODE -ne 0) { throw "Falha ao instalar aqtinstall." }
        & python -m aqt install-qt windows desktop $QtVersion $QtArch -O $QtBase
        if ($LASTEXITCODE -ne 0) { throw "Falha ao baixar Qt $QtVersion ($QtArch). Você pode definir ATENA_QT_ROOT para uma instalação Qt 6.4+." }
    }
}
if (-not (Test-Path (Join-Path $QtRoot "bin\windeployqt.exe"))) { throw "Qt inválido em $QtRoot" }

$env:VCPKG_ROOT = $VcpkgRoot
$env:ATENA_QT_ROOT = $QtRoot

$State = @{
    VCPKG_ROOT = $VcpkgRoot
    ATENA_QT_ROOT = $QtRoot
    QT_VERSION = $QtVersion
    VISUAL_STUDIO = $vs
} | ConvertTo-Json
$State | Set-Content -Encoding UTF8 (Join-Path $Deps "windows-env.json")

Write-Host "Dependências preparadas." -ForegroundColor Green
Write-Host "VCPKG_ROOT=$VcpkgRoot"
Write-Host "ATENA_QT_ROOT=$QtRoot"
