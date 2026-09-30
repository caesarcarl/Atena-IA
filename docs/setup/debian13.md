# Atena 0.5.4 - Debian 13 Ready

Base consolidada para Debian 13/trixie. Esta árvore já incorpora os hotfixes usados durante a depuração:

- migration SQLite intermediária/legada;
- Core desacoplado da persistência de providers em runtime;
- correção do CMake do teste ausente;
- identidade modular com `mission.json`;
- RAG/PDF via worker Python opcional no desktop;
- perfil mobile com worker Python desativado;
- CLI/IPC/Core da linha 0.5.

## Instalação de desenvolvimento em uma VM Debian limpa

```bash
cd Atena-0.5.4-Debian13-Ready
./scripts/bootstrap-debian13.sh
```

## Teste inicial

```bash
./build/atena --demo
```

Na CLI:

```text
/status
/doctor
/runtime
/providers
/identity
```

## RAG/PDF

No desktop, `python3` + `pdftotext` são instalados pelo bootstrap. Use:

```text
/rag add /caminho/arquivo.pdf
/rag list
```

## Mobile

O build mobile não habilita o worker Python:

```bash
./scripts/build-mobile-core.sh
```

A ideia é manter Core/Runtime/RAG existentes em C/C++ e preparar/transportar índices quando a ingestão pesada não fizer sentido no aparelho.

## Importante

`build/` não é distribuído no ZIP. Sempre compile no computador de destino. Não copie `~/.local/share/atena/atena.db` de outra instalação para uma VM de teste limpa se o objetivo for validar instalação nova.
