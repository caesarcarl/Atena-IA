# Status da base 0.5

## Verificado nesta geração

- CMake configure: OK
- build Linux sem UI: OK
- testes: 8/8 PASS
- worker Python `health`: OK
- worker Python `chunk`: OK
- CLI `--help`: OK

## Mantido do 0.4.3

Core, IPC, SDK, SQLite, RAG FTS5, Ollama, OpenAI-compatible, tools, packaging e fonte da UI Qt.

## Novo

Runtime C++, tuning Ollama adaptativo, teste runtime, CLI reorganizada, worker Python e documentação de migração.

## Próximo alvo funcional

1. testar conversa Ollama real no hardware de entrega;
2. adicionar `/api/ps` + métricas de inferência;
3. Context Planner por tokens;
4. ingestão RAG por arquivo via IPC;
5. backend llama.cpp como alternativa ao Ollama.
