# Atena 0.4.2-2 — Assets Funcionais v1 integrados

Esta revisão preserva as correções UI/Core da 0.4.2 e substitui a identidade visual anterior pelo pacote `Atena_Assets_Funcionais_v1`.

## Integração realizada

- Branding novo embutido no Qt Resource System (`atena.qrc`).
- `Q_INIT_RESOURCE(atena)` adicionado ao executável, garantindo o registro dos recursos mesmo com `atena-ui-lib` estática.
- Ícone global da aplicação e ícone da janela usam o novo `atena-app-icon.png`.
- Logo horizontal nova integrada à barra lateral e ao onboarding.
- Home usa `home-hero.png` e os quatro ícones de ações rápidas.
- Onboarding usa `athena-about-onboarding.png` e ícones Local/Cloud/Híbrido.
- Conhecimento usa `library-knowledge.png` e `add-knowledge.png`.
- Modelos e Providers usam `models-providers.png` e os novos ícones correspondentes.
- Navegação principal usa os PNGs novos de modelos, providers, biblioteca, tools, privacidade, diagnóstico e configurações.
- O ícone de anexar usa `files.png`.
- Os ícones utilitários Enviar/Parar foram preservados porque o novo pack não fornece equivalentes.

## Integração Linux / Debian

- Ícones 16, 24, 32, 48, 64, 96, 128, 256 e 512 px são instalados no tema `hicolor` com o nome `atena`.
- Todos os assets fornecidos também são instalados em `/usr/share/atena/assets`.
- Os mesmos assets ficam dentro do código-fonte instalado pelo `.deb` e são compilados no binário Qt via `.qrc` durante o `postinst`.
- Nenhum download remoto de imagens é necessário em tempo de execução.

## Validação

- 6/6 testes backend aprovados após a integração visual.
- `atena.qrc` validado: todos os arquivos declarados existem.
- Todas as referências `:/...` usadas pelo código apontam para recursos declarados no QRC.
- O ambiente de geração não contém Qt 6 SDK; a UI é compilada no computador de destino pelo `.deb`, como na entrega 0.4.2 anterior.
