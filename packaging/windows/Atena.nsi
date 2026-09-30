!include "MUI2.nsh"
!include "FileFunc.nsh"

!ifndef ROOT
!error "ROOT não definido"
!endif
!ifndef STAGE
!error "STAGE não definido"
!endif

Name "Atena IA 0.4.3"
OutFile "${ROOT}\Atena-Setup-0.4.3-x64.exe"
InstallDir "$LOCALAPPDATA\Programs\Atena"
InstallDirRegKey HKCU "Software\Atena" "InstallDir"
RequestExecutionLevel user
Unicode True
Icon "${ROOT}\ui\qt\resources\windows\atena.ico"
UninstallIcon "${ROOT}\ui\qt\resources\windows\atena.ico"

VIProductVersion "0.4.3.0"
VIAddVersionKey "ProductName" "Atena IA"
VIAddVersionKey "CompanyName" "Projeto Athenas OS"
VIAddVersionKey "FileDescription" "Instalador Atena IA para Windows x64"
VIAddVersionKey "FileVersion" "0.4.3"
VIAddVersionKey "ProductVersion" "0.4.3"

!define MUI_ABORTWARNING
!define MUI_FINISHPAGE_RUN "$INSTDIR\bin\atena-ui.exe"
!define MUI_FINISHPAGE_RUN_TEXT "Abrir Atena IA"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "PortugueseBR"

Section "Atena IA" SEC01
  ; Stop old processes from a previous build so binaries can be replaced safely.
  nsExec::ExecToLog 'taskkill /IM atena-ui.exe /F'
  nsExec::ExecToLog 'taskkill /IM atena-core.exe /F'

  SetOutPath "$INSTDIR"
  File /r "${STAGE}\*.*"

  WriteRegStr HKCU "Software\Atena" "InstallDir" "$INSTDIR"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Atena" "DisplayName" "Atena IA"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Atena" "DisplayVersion" "0.4.3"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Atena" "Publisher" "Projeto Athenas OS"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Atena" "InstallLocation" "$INSTDIR"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Atena" "DisplayIcon" "$INSTDIR\bin\atena-ui.exe"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Atena" "UninstallString" '"$INSTDIR\Uninstall.exe"'
  WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Atena" "NoModify" 1
  WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Atena" "NoRepair" 1

  CreateDirectory "$SMPROGRAMS\Atena"
  CreateShortcut "$SMPROGRAMS\Atena\Atena IA.lnk" "$INSTDIR\bin\atena-ui.exe" "" "$INSTDIR\bin\atena-ui.exe" 0
  CreateShortcut "$SMPROGRAMS\Atena\Atena CLI.lnk" "$INSTDIR\bin\atena.exe" "" "$INSTDIR\bin\atena-ui.exe" 0
  CreateShortcut "$DESKTOP\Atena IA.lnk" "$INSTDIR\bin\atena-ui.exe" "" "$INSTDIR\bin\atena-ui.exe" 0
  WriteUninstaller "$INSTDIR\Uninstall.exe"
SectionEnd

Section "Uninstall"
  nsExec::ExecToLog 'taskkill /IM atena-ui.exe /F'
  nsExec::ExecToLog 'taskkill /IM atena-core.exe /F'

  Delete "$DESKTOP\Atena IA.lnk"
  Delete "$SMPROGRAMS\Atena\Atena IA.lnk"
  Delete "$SMPROGRAMS\Atena\Atena CLI.lnk"
  RMDir "$SMPROGRAMS\Atena"

  DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Atena"
  DeleteRegKey HKCU "Software\Atena"

  ; Preserve %LOCALAPPDATA%\Atena and %APPDATA%\Atena so conversations,
  ; preferences and local knowledge are not deleted by uninstalling the app.
  RMDir /r "$INSTDIR"
SectionEnd
