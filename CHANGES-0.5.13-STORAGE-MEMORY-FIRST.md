# Atena 0.5.13 - Storage memory-first

- O runtime iniciado pela UI usa armazenamento efêmero em memória por padrão.
- Bancos SQLite antigos deixam de bloquear provider, modelo, sessão ou chat.
- `ATENA_STORAGE_MODE=persistent` reativa explicitamente o arquivo `atena.db` para desenvolvimento/migração.
- O Core aceita `--storage memory|persistent`.
- O log de spawn informa `storage=memory` e `database=:memory:` quando aplicável.
- A mudança é transitória: o próximo passo separa conhecimento/RAG persistente do estado efêmero de conversa/runtime.
