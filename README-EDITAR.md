# Atena 0.4.3 — Fonte Completo Robusto

Esta é a árvore de código-fonte unificada para edição e desenvolvimento.

## Arquitetura

UI Qt 6 -> RealAtenaClient -> SDK C -> IPC -> Atena Core -> Providers/Tools/RAG/SQLite

## Plataformas

- Linux: transporte Unix, libsecret quando disponível, scripts Debian/Mint e systemd.
- Windows x64: Named Pipes, CreateProcessW, Windows Credential Manager, scripts PowerShell/NSIS.

## Providers implementados nesta baseline

- Ollama
- OpenAI-compatible
- Presets OpenAI/Groq/DeepSeek/xAI sobre protocolo OpenAI-compatible
- Mock para testes

Anthropic e Gemini nativos exigem adapters próprios e não são tratados como implementados nesta baseline.

## Testes da baseline

Backend compilado do zero com UI desativada e 6/6 testes CTest aprovados:
- test_core
- test_persistence
- test_rag
- test_ipc_sdk
- test_tools
- test_protocol

## Edição

Os principais pontos são:
- `core/`: orquestração e regras do Core
- `ipc/`: servidor e dispatcher IPC
- `sdk/`: cliente do Core usado por CLI/UI
- `providers/`: adapters de IA
- `platform/`: paths/process/secrets/transporte por SO
- `ui/qt/`: interface Qt 6
- `memory/`: SQLite/persistência
- `tools/`: ferramentas e policy
- `identity/`: identidade/pedagogia/regional
- `tests/`: testes automatizados

Não há binários, `.deb`, `.exe`, DLLs ou diretórios de build neste pacote.
