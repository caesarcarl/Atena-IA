@echo off
setlocal
cd /d "%~dp0"
echo ============================================================
echo      ATENA IA 0.4.3 - BUILDER WINDOWS x64
echo ============================================================
echo.
echo Este processo prepara as dependencias, compila Core + CLI + UI
echo e gera o ZIP portatil e, quando NSIS estiver disponivel, o Setup EXE.
echo.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\package-windows.ps1" -InstallSystemTools
if errorlevel 1 (
  echo.
  echo BUILD INTERROMPIDA. Veja a mensagem acima.
  pause
  exit /b 1
)
echo.
echo BUILD CONCLUIDA.
echo Procure por:
echo   Atena-Setup-0.4.3-x64.exe
echo   dist\Atena-0.4.3-Windows-x64-Portable.zip
echo.
pause
