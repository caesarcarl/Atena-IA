# Atena Knowledge Base v0.1

Pacote local de conhecimento do projeto Atena.

- `pdf/`: fontes originais para ingestão no desktop com o worker Python/Poppler.
- `txt/`: extração textual portátil, útil inclusive em builds mobile sem Python.

## Desktop

Na raiz do projeto Atena:

```bash
ATENA_BIN=./build/atena ./knowledge/ingest-desktop.sh
```

## Mobile / build sem Python

```bash
ATENA_BIN=./build/atena ./knowledge/ingest-portable.sh
```

Os dados são indexados localmente pelo Core. Nenhum conteúdo é enviado a um servidor por estes scripts.
