# Atena 0.5.2 - hotfix de migração SQLite

Corrige o caso real em que o Core compilava e todos os testes passavam, mas a CLI terminava em `timeout` porque o processo `atena-core` morria antes de criar o socket IPC.

## Causa confirmada

Bancos de versões intermediárias podiam ter nomes de tabelas atuais com colunas antigas. `CREATE TABLE IF NOT EXISTS` não altera uma tabela já existente. Isso deixou instalações com combinações como:

- `messages.conversation_id` em vez de `messages.session_id`;
- `preferences` sem `value` e/ou `updated_at`;
- `providers` sem colunas exigidas pelo provider atual.

O efeito era `database_error` durante a abertura do armazenamento ou durante `atena_core_provider_configure(... ollama ...)`.

## Correções

- schema version 4;
- reparo idempotente de `preferences`;
- migração best-effort de aliases `data`, `payload`, `pref_value`, `json` e `text` para `value`;
- preservação da tabela antiga quando o formato é desconhecido (`preferences_legacy_v4*`);
- reparo aditivo de `providers`;
- preservação de tabela `providers` desconhecida quando não possui `id`;
- mantém a migração `conversation_id -> session_id`;
- novo `test_migration_intermediate`, reproduzindo o erro observado numa instalação real;
- startup debug da 0.5.1 preservado.

Nunca apaga automaticamente o banco do usuário.
