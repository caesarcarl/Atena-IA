# Mapa de responsabilidade - Atena 0.5

```text
CLI / UI / Android Bridge
          |
          v
        IPC SDK
          |
          v
+-----------------------+
|      ATENA CORE       |
| orchestrator/context  |
| memory/policy/tools   |
+-----------+-----------+
            |
   +--------+---------+----------------+
   |                  |                |
   v                  v                v
Runtime            Providers          RAG
C/C++              Ollama/cloud       SQLite + worker
   |                  |                |
   +------------------+----------------+
                      |
                      v
                     LLM
```

## Quem faz o quê

### Core C
Coordena sessão, contexto, provider, eventos, cancelamento, tools e persistência. Não deve conhecer detalhes HTTP de cada API.

### Runtime C++
Decide envelope de execução a partir de RAM disponível, CPU, swap e benchmarks. É a evolução direta da lógica de `resources.py`/`manager.py` do CyberCore.

### Ollama provider
Continua como backend local simples. Nesta base já recebe `num_ctx`, `num_thread` e `keep_alive` do Runtime.

### Native runtime
Futuro backend llama.cpp/GGUF. Deve oferecer controle de mmap, KV cache, threads, batch e offload sem tornar o resto do Atena dependente de llama.cpp.

### Python worker
Processamento pesado opcional: parsing, chunking, embeddings, reranking e ingestão em lote. Nunca é requisito para chat básico.

### RAG
Começa com FTS5 existente. Evolução: FTS + vetores + fusão + reranking + proveniência.

### Identity/Pedagogy
Define Atena independentemente do modelo. A próxima etapa é mover a pedagogia socrática de JSON/prompt para uma máquina de estados do Core.

### Agents/Tools
Ações reais do sistema passam por registry/policy. Shell digitado diretamente pelo usuário na CLI é diferente de shell proposto por LLM.

## Migração CyberCore -> Atena

| CyberCore | Atena 0.5 |
|---|---|
| `core/resources.py` | `runtime/runtime.cpp` + `runtime.h` |
| `llm/manager.py` | futuro `runtime/scheduler/` |
| `llm/ollama.py` | `providers/ollama/ollama_provider.c` |
| `llm/types.py` métricas | `AtenaMetrics` + futuro benchmark DB |
| `core/engine.py` | `core/core.c`, sem segundo Core |
| CLI Python | `cli/main.c` |
| RAG Python vazio | `memory/store.c` + `python/atena_worker` |
