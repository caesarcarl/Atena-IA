# Build da Atena 0.4.3 no Windows

## Requisitos

O caminho mais simples é executar `PREPARAR-WINDOWS.bat`. O build usa:

- Windows 10 ou 11 x64
- Visual Studio 2022 Build Tools, workload Desktop development with C++
- CMake
- Git
- Python 3
- Qt 6.4+ para MSVC 2022 x64
- vcpkg
- NSIS para gerar o setup `.exe`

Qt e vcpkg podem ficar dentro de `.deps/` do próprio projeto. Não precisam ser instalados globalmente.

## Build manual

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\bootstrap-windows.ps1 -InstallSystemTools
powershell -ExecutionPolicy Bypass -File .\scripts\build-windows.ps1
powershell -ExecutionPolicy Bypass -File .\scripts\package-windows.ps1
```

## Saída

```text
build-windows\                       build MSVC
dist\Atena-0.4.3-Windows-x64\       staging completo
dist\Atena-0.4.3-Windows-x64-Portable.zip
Atena-Setup-0.4.3-x64.exe
```

## Diagnóstico

Se a UI disser que o Core está offline, verifique:

```powershell
Get-Content "$env:LOCALAPPDATA\Atena\core.log" -Tail 200
Get-Process atena-core -ErrorAction SilentlyContinue
```

O Core e a UI precisam estar na mesma pasta `bin` no pacote final, junto com `identity/`.

## APIs

As chaves são persistidas com `CredWriteW`/`CredReadW` no Windows Credential Manager. O Core nunca precisa gravar a chave em `atena.db`.
