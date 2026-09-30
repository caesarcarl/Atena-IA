# Mudanças da base integrada 0.5

- preservado o Core 0.4.3 em vez de reescrever o projeto;
- removidos artefatos `build-dev`/`build-audit` do pacote-fonte;
- CMake agora habilita C e C++;
- criado `atena_runtime` C++ com ABI C pública;
- status do Core agora inclui snapshot de recursos e plano runtime;
- provider Ollama recebe tuning adaptativo de `num_ctx`, `num_thread` e `keep_alive`;
- CLI foi reorganizada como REPL persistente e ganhou `/runtime`, `/doctor`, `/models`, `/model`, `/use` e `!comando`;
- criado worker Python opcional JSONL com `health`, `resources`, `chunk` e `ingest_file` textual;
- pedagogia atualizada para método socrático adaptativo;
- CyberCore original preservado apenas em `reference/` para consulta durante a migração;
- criados mapas de arquitetura, sprint e status explícito do que é funcional versus planejado;
- adicionado `test_runtime`; total atual: 8 testes.
