# Relatório de port Windows 0.4.3

Principais correções desta revisão:

1. Dispatcher IPC unificado entre Unix e Win32.
2. `providers.put`, `providers.test`, `models.list`, `models.select`, `models.pull` e `models.remove` disponíveis também no Named Pipe do Windows.
3. `atena-core.exe` localizado como irmão da UI/CLI e iniciado por `CreateProcessW`.
4. stdout/stderr do Core gravados em `%LOCALAPPDATA%\Atena\core.log`.
5. Pipe nomeado por SID do usuário e ACL restrita ao usuário.
6. API keys no Windows Credential Manager.
7. Ícone `.ico` multi-resolução e metadata de executável Windows.
8. Pipeline MSVC 2022 + vcpkg + Qt + windeployqt.
9. Pacote portátil e setup NSIS por usuário.
10. Assets Qt preservados e compilados no `.qrc`.

A parte compartilhada C17 foi validada com 6/6 testes após a refatoração.
