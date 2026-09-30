param([string]$Stage = "")
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot

$requiredMethods = @("providers.put","providers.test","models.list","models.select","models.pull","models.remove","chat.start")
$dispatch = Get-Content "$Root\ipc\server_dispatch.c" -Raw
foreach ($m in $requiredMethods) { if ($dispatch -notmatch [regex]::Escape($m)) { throw "Método IPC ausente no dispatcher compartilhado: $m" } }

foreach ($source in @("$Root\platform\transport_win32.c", "$Root\platform\process.c", "$Root\ipc\server_win32.c")) {
    if (Select-String -Path $source -Pattern "ATENA_ERR_UNSUPPORTED") { throw "Stub Win32 ainda presente em $source" }
}
if ((Get-Content "$Root\platform\secrets.c" -Raw) -notmatch "CredWriteW") { throw "Windows Credential Manager não está integrado." }
if ((Get-Content "$Root\platform\transport_win32.c" -Raw) -notmatch "WaitNamedPipeA") { throw "Named Pipe client não está integrado." }
if ((Get-Content "$Root\ipc\server_win32.c" -Raw) -notmatch "CreateNamedPipeA") { throw "Named Pipe server não está integrado." }

if ($Stage) {
    $Bin = Join-Path $Stage "bin"
    foreach ($f in @("atena-ui.exe","atena-core.exe","atena.exe")) {
        if (-not (Test-Path (Join-Path $Bin $f))) { throw "Artefato ausente: $f" }
    }
}
Write-Host "Gate Windows: OK" -ForegroundColor Green
