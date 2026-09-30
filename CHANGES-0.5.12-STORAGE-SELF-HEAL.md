# Atena 0.5.12 - Storage self-heal

- Repara bancos legados onde `preferences.key` e `providers.id` existem, mas não são PRIMARY KEY.
- Preserva as tabelas incompatíveis como `*_legacy_v4*` antes de reconstruir o esquema canônico.
- Migra metadados reconhecíveis de preferências e providers para as novas tabelas.
- Eleva `PRAGMA user_version` para 5.
- Adiciona diagnóstico explícito para falhas de prepare/step em persistência.
- Adiciona regressão `test_migration_constraints`.
- Diagnóstico da UI mostra apenas o spawn atual do Core, sem misturar erros históricos do `core.log`.
