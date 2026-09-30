# Comece aqui: Atena 0.5

## Regra principal

Não reescreva o Core inteiro. Esta base foi montada para migrar em camadas, mantendo algo executável durante o desenvolvimento.

## Ordem recomendada

1. Rode `./scripts/dev-build-0.5.sh` e mantenha os 8 testes verdes.
2. Teste `./build/atena --demo` antes de alterar providers.
3. Teste Ollama real e compare `/status` antes/depois da inferência.
4. Evolua `runtime/` e meça antes de trocar parâmetros.
5. Crie o backend llama.cpp atrás de uma nova interface de runtime, sem remover Ollama.
6. Evolua o RAG lexical existente; não descarte o SQLite/FTS5.
7. Conecte o worker Python somente por IPC/subprocess JSONL.
8. Só então adicione providers nativos e agentes do sistema.

## Arquivos mais importantes

- `core/core.c`: orquestração atual.
- `core/context.c`: identidade, histórico e RAG enviados ao modelo.
- `include/atena/provider.h`: contrato dos providers.
- `providers/ollama/ollama_provider.c`: inferência local via Ollama e tuning adaptativo.
- `runtime/runtime.cpp`: lógica migrada do CyberCore para C++ nativo.
- `include/atena/runtime.h`: ABI C do runtime.
- `memory/store.c`: SQLite, sessões, memória e RAG lexical.
- `cli/main.c`: REPL persistente.
- `python/atena_worker/`: worker opcional.
- `reference/cybercore-lite/`: código original do CyberCore apenas para consulta durante a migração.

## Não faça agora

- não venda `llama.cpp` como integrado antes de o backend existir;
- não coloque Python dentro do processo do Core;
- não dê root/bash irrestrito ao LLM;
- não misture lógica específica de OpenAI/Gemini/Anthropic dentro de `core/core.c`;
- não coloque build gerado no ZIP final.
