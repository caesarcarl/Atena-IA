# Atena 0.5.6 - CPU AutoTune

- Runtime considera RAM disponível, proporção de memória, uso de swap e carga da CPU.
- Reserva 1-2 CPUs lógicas para desktop, IPC, RAG e helpers.
- Perfil balanced usa até 8 threads quando há folga.
- `num_batch` adaptativo: 64 / 128 / 256 / 512.
- `keep_alive` adaptativo: 0 / 60 / 180 / 300 segundos.
- Python worker só é permitido no balanced quando há memória e CPU suficientes.
- Ollama recebe `num_ctx`, `num_thread`, `num_batch` e `keep_alive` por requisição.
- Thinking local desativado por padrão; `ATENA_OLLAMA_THINK=1` reativa.
- Overrides: `ATENA_OLLAMA_NUM_CTX`, `ATENA_OLLAMA_NUM_THREAD`, `ATENA_OLLAMA_NUM_BATCH`, `ATENA_OLLAMA_KEEP_ALIVE`.
- Scripts de ajuste do serviço Ollama e benchmark de threads.
