# Atena 0.5.6 no Linux Mint

Versão consolidada com os hotfixes de banco/Core, RAG/PDF, identidade de programação e AutoTune de CPU/RAM.

## Build rápido

```bash
chmod +x scripts/bootstrap-mint.sh
./scripts/bootstrap-mint.sh
```

## Teste

```bash
./build/atena --demo
```

Dentro da CLI:

```text
/doctor
/runtime
/identity
/providers
/rag list
```

## Ollama CPU-only

Depois que o Ollama estiver instalado:

```bash
./scripts/benchmark-ollama-cpu.sh qwen3:0.6b
./scripts/configure-ollama-cpu.sh
./build/atena
```

O AutoTune controla threads, contexto, batch, keep-alive e habilitação do worker Python conforme a pressão real de CPU/RAM.
