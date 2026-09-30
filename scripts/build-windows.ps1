param(
    [ValidateSet("Debug","Release")][string]$Configuration = "Release",
    [string]$QtVersion = "6.8.3",
    [switch]$InstallSystemTools,
    [switch]$Clean
)
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
& "$PSScriptRoot\bootstrap-windows.ps1" -QtVersion $QtVersion -InstallSystemTools:$InstallSystemTools

$Deps = Join-Path $Root ".deps"
$State = Get-Content (Join-Path $Deps "windows-env.json") -Raw | ConvertFrom-Json
$VcpkgRoot = [string]$State.VCPKG_ROOT
$QtRoot = [string]$State.ATENA_QT_ROOT
$Toolchain = Join-Path $VcpkgRoot "scripts\buildsystems\vcpkg.cmake"
$Build = Join-Path $Root "build-windows"
$VcpkgInstalled = Join-Path $Deps "vcpkg_installed"
if ($Clean -and (Test-Path $Build)) { Remove-Item $Build -Recurse -Force }

$cmakeArgs = @(
    "-S", $Root,
    "-B", $Build,
    "-G", "Visual Studio 17 2022",
    "-A", "x64",
    "-DCMAKE_TOOLCHAIN_FILE=$Toolchain",
    "-DVCPKG_MANIFEST_DIR=$Root\packaging\windows",
    "-DVCPKG_INSTALLED_DIR=$VcpkgInstalled",
    "-DVCPKG_TARGET_TRIPLET=x64-windows",
    "-DCMAKE_PREFIX_PATH=$QtRoot",
    "-DATENA_BUILD_UI=ON",
    "-DATENA_REQUIRE_UI=ON",
    "-DATENA_BUILD_TESTS=OFF",
    "-DATENA_PACKAGE_REVISION=0.4.3-win1"
)
& cmake @cmakeArgs
if ($LASTEXITCODE -ne 0) { throw "Configuração CMake falhou." }
& cmake --build $Build --config $Configuration --parallel
if ($LASTEXITCODE -ne 0) { throw "Compilação Windows falhou." }

$required = @(
    (Join-Path $Build "$Configuration\atena.exe"),
    (Join-Path $Build "$Configuration\atena-core.exe")
)
$uiCandidates = @(
    (Join-Path $Build "ui\qt\$Configuration\atena-ui.exe"),
    (Join-Path $Build "$Configuration\atena-ui.exe")
)
foreach ($f in $required) { if (-not (Test-Path $f)) { throw "Artefato ausente: $f" } }
if (-not ($uiCandidates | Where-Object { Test-Path $_ })) { throw "atena-ui.exe não foi produzido." }
Write-Host "Build Windows x64 concluído." -ForegroundColor Green
