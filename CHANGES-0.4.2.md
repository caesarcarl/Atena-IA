# Atena 0.4.2 — UI/Core Integration Fix

Esta entrega preserva o Core C17 existente e corrige a ponte da UI Qt para o Core.

## Corrigido

- `atena-ui` passa a carregar um `atena-core` irmão em builds de desenvolvimento.
- A identidade também é resolvida ao lado do executável antes do caminho instalado.
- `providers.put` e `providers.test` foram implementados para Ollama.
- Endpoint e modelo padrão do Ollama são persistidos no SQLite por preferências do Core.
- Token Bearer opcional para Ollama remoto/proxy é aceito apenas na sessão atual e não é persistido.
- `models.list`, `models.select`, `models.pull` e `models.remove` foram implementados para Ollama.
- A página Modelos lista os modelos reais de `/api/tags`, permite baixar, selecionar e remover.
- A UI não chama teste de provider antes da configuração terminar.
- A UI passa a interpretar `enabled`/`configured` de forma consistente.
- Erros de streaming `operation.finished(status=failed)` passam a retornar erro real ao SDK/UI.
- Versão interna atualizada para 0.4.2.

## Escopo desta entrega

Ollama continua sendo o provider local principal. Esta revisão também inclui um adapter **OpenAI-compatible** com presets para OpenAI, Groq, DeepSeek e xAI. Em Linux, chaves podem ser persistidas no keyring via libsecret quando o pacote é compilado com `libsecret-1-dev`. Anthropic e Gemini nativos continuam fora do escopo desta revisão porque usam contratos próprios.

## Testes executados

- 6/6 testes C originais aprovados.
- Teste IPC adicional com servidor Ollama simulado: configuração, teste, listagem de modelos, seleção, pull, remoção e chat streaming aprovados.


## Revisão Debian 0.4.2-3 — Core incluído e inicialização determinística

- O `.deb` passa a carregar o **código-fonte completo do Core** e compila Core + CLI + UI no computador de destino para evitar incompatibilidade de glibc entre Debian 13 e Mint/Ubuntu.
- A instalação só termina com sucesso se existirem os três executáveis: `atena`, `atena-core` e `atena-ui`.
- `atena-core` é instalado em `/usr/libexec/atena/atena-core`.
- `atena-ui` passa a ser um launcher estável em `/usr/bin/atena-ui`, que fixa `ATENA_CORE_EXECUTABLE` e `ATENA_IDENTITY_DIR` antes de abrir o binário Qt em `/usr/libexec/atena/atena-ui-bin`.
- O launcher recusa abrir uma UI órfã caso o Core esteja ausente, em vez de exibir uma aplicação aparentemente instalada mas permanentemente offline.
- O processo do Core grava inicialização e falhas em `~/.local/share/atena/core.log`.
- O SDK valida `system.hello` depois de conectar e aguarda até 10 segundos pelo Core recém-iniciado.
- A configuração CMake ganhou `ATENA_REQUIRE_UI=ON`, usada pelo `.deb` para impedir instalação parcial sem Qt.
- Dependência de desenvolvimento `libsecret-1-dev` adicionada ao `.deb` para compilar armazenamento seguro de chaves de API no keyring do Linux.
- Teste ponta a ponta validado em layout instalado simulado: SDK → spawn do Core → IPC → Ollama falso → `POST /api/chat` → streaming `AUTO SPAWN OK`.
