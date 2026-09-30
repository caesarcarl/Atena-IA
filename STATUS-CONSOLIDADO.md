# Atena 0.5.4 Debian 13 Ready - estado consolidado

Esta entrega consolida a base 0.5 com os hotfixes aplicados durante a depuração real:

1. migração SQLite compatível com banco intermediário/legado;
2. runtime de provider desacoplado da persistência, evitando derrubar o Core por falha de banco;
3. correção do CMake do teste ausente;
4. identidade modular com `mission.json` e comando `/identity`;
5. RAG com ingestão de texto e PDF no desktop por worker Python/Poppler;
6. build mobile sem worker Python;
7. Runtime adaptativo C++ e CLI/IPC/Core preservados.

Validação da árvore consolidada antes do empacotamento:

- CMake + Ninja: OK
- 9/9 testes: PASS
- primeira inicialização com HOME/XDG vazios: OK
- `/status`: OK
- `/identity`: OK
- provider mock + IPC: OK

O ZIP não contém `build/`. Compile no Debian de destino com:

```bash
./scripts/bootstrap-debian13.sh
```
