# Atena 0.5.7 - Benchmark CPU adaptativo

- benchmark Ollama não tenta mais 4/6/8/10 threads cegamente;
- candidatos limitados ao número real de CPUs lógicas;
- Celeron/2 CPUs testa apenas 1 e 2 threads;
- contexto e batch do benchmark adaptam-se à RAM disponível;
- warm-up separa custo de carregamento do modelo do throughput de geração;
- timeout explícito evita benchmark aparentemente congelado;
- progresso por teste fica visível;
- mantém overrides `ATENA_BENCH_THREADS`, `ATENA_BENCH_CTX` e `ATENA_BENCH_BATCH`.
