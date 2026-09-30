# Atena IA 0.4.3 — Windows x64 + Linux

Atena é a assistente local/híbrida do projeto Athenas OS. Esta revisão mantém um único Core C17 para Linux e Windows e usa a mesma API IPC `atena.ipc/2` nas duas plataformas.

## Windows x64

A versão Windows é composta por:

```text
atena-ui.exe    interface Qt 6
atena-core.exe  Core C17
atena.exe       CLI
identity/       persona e identidade
```

A UI e a CLI iniciam `atena-core.exe` automaticamente. No Windows a comunicação ocorre por **Named Pipe privado do usuário**, com nome derivado do SID do usuário.

Dados locais:

```text
%LOCALAPPDATA%\Atena\atena.db
%LOCALAPPDATA%\Atena\core.log
%LOCALAPPDATA%\Atena\Cache
%LOCALAPPDATA%\Atena\Runtime
%APPDATA%\Atena
```

Chaves de API são armazenadas no **Windows Credential Manager**, e não no SQLite/QSettings.

### Providers implementados

- Ollama local ou remoto
- OpenAI
- Groq
- DeepSeek
- xAI
- endpoint OpenAI-compatible personalizado

OpenAI, Groq, DeepSeek e xAI usam o adapter OpenAI-compatible. Anthropic e Gemini nativos ainda exigem adapters próprios.

## Como gerar o executável/instalador no Windows

Em Windows 10/11 x64, extraia o projeto e execute:

```text
PREPARAR-WINDOWS.bat
GERAR-INSTALADOR-WINDOWS.bat
```

`PREPARAR-WINDOWS.bat` pode instalar/verificar Visual Studio 2022 Build Tools, CMake, Git, Python e NSIS e prepara vcpkg + Qt.

O segundo script produz:

```text
Atena-Setup-0.4.3-x64.exe
dist\Atena-0.4.3-Windows-x64-Portable.zip
```

O instalador é por usuário e instala em:

```text
%LOCALAPPDATA%\Programs\Atena
```

## Arquitetura

```text
Qt UI / CLI
    ↓
Atena SDK
    ↓
atena.ipc/2
    ↓
Named Pipe (Windows) / Unix socket (Linux)
    ↓
Atena Core C17
    ↓
Providers / SQLite / RAG / Tools / Identity
```

O dispatcher IPC é compartilhado por Linux e Windows. Isso evita que o Core Windows fique atrás do Core Linux em métodos como `providers.put`, `providers.test` e `models.*`.

## Testes

A árvore compartilhada desta entrega foi recompilada no ambiente Linux após a refatoração Win32 e passou:

```text
test_core          PASS
test_persistence   PASS
test_rag           PASS
test_ipc_sdk       PASS
test_tools         PASS
test_protocol      PASS

6/6 PASS
```

A compilação final do `.exe` requer Windows/MSVC/Qt, portanto deve ser executada em um host Windows ou CI Windows.
