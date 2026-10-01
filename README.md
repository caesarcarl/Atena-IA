# Atena 0.5 Base Integrada

Base de desenvolvimento que preserva o Atena 0.4.3 funcional e absorve a lógica útil do CyberCore Lite na camada de runtime.

## O que já funciona nesta base

- Core C, IPC, SDK e persistência SQLite herdados do Atena 0.4.3.
- Provider Ollama e provider OpenAI-compatible herdados.
- CLI persistente com chat, status, providers, modelos, troca de modelo e shell explícito `!comando`.
- Runtime adaptativo C++ com leitura real de RAM, swap, CPU, load average e arquitetura.
- Plano de execução `emergency/constrained/balanced/performance` sem amarrar o perfil a um modelo específico.
- Ollama usa automaticamente `num_ctx`, `num_thread` e `keep_alive` sugeridos pelo Runtime, com overrides por ambiente.
- RAG lexical existente do Atena continua funcionando.
- Worker Python opcional com protocolo JSONL, chunking e ingestão de arquivos texto/código.
- Pedagogia socrática declarada em `identity/pedagogy.json`.
- 8 testes de Core/runtime passando em build limpa no Linux.

## O que é estrutura, ainda não implementação final

- runtime nativo `llama.cpp`/GGUF;
- RAG vetorial + reranker;
- providers nativos Anthropic, Gemini e OpenAI Responses;
- Android Bridge / Desktop Agent;
- Context Planner por tokens;
- Pedagogy Engine como máquina de estados executável.

Esses diretórios existem como destino arquitetural e estão explicitamente marcados para não parecer funcionalidade pronta.

## Build rápida no Debian/Ubuntu/Mint

Dependências de desenvolvimento:

```bash
sudo apt install build-essential cmake ninja-build pkg-config \
  libsqlite3-dev libjson-c-dev libcurl4-openssl-dev libssl-dev
```

Build:

```bash
./scripts/dev-build-0.5.sh
```

Ou manualmente:

```bash
cmake -S . -B build -G Ninja -DATENA_BUILD_UI=OFF -DATENA_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

CLI:

```bash
./build/atena
```

Com Ollama:

```bash
ollama serve
./build/atena run qwen3:0.6b "explique ponteiros"
```

## Tuning do Ollama

Por padrão o Runtime escolhe `num_ctx`, `num_thread` e `keep_alive` com base nos recursos atuais.

Overrides:

```bash
ATENA_OLLAMA_NUM_CTX=4096 ./build/atena
ATENA_OLLAMA_NUM_THREAD=2 ./build/atena
ATENA_DISABLE_RUNTIME_TUNING=1 ./build/atena
```

## Worker Python

Não é necessário para o Core iniciar.

```bash
cd python/atena_worker
printf '{"op":"health"}\n' | PYTHONPATH=. python3 -m atena_worker
```

Veja `COMECE-AQUI-0.5.md` e `docs/architecture/MAPA-ATENA-0.5.md`.
