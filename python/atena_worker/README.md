# Atena Worker 0.5.4

Worker Python opcional do perfil **desktop**. O Core não depende dele para iniciar.

Suporta:

- TXT/Markdown/código via biblioteca padrão;
- PDF via `pdftotext` (Poppler), com fallback opcional para `pypdf`;
- protocolo JSONL em stdin/stdout;
- modo direto usado pela CLI: `python3 -m atena_worker ingest-file arquivo.pdf`.

Teste sem instalar:

```bash
cd python/atena_worker
printf '{"op":"health"}\n' | PYTHONPATH=. python3 -m atena_worker
PYTHONPATH=. python3 -m atena_worker ingest-file ../../manual.pdf
```

No build mobile o worker pode ser desativado com `-DATENA_MOBILE=ON`. O índice RAG
continua consultável pelo Core C/C++.
