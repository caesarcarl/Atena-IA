# UI Qt 6 no Debian

A edição visual está concentrada em `ui/qt/`.

## Pontos principais

- `src/MainWindow.*`: shell principal, sidebar, navegação e status do Core.
- `src/pages/`: páginas funcionais.
- `src/dialogs/`: onboarding, providers e confirmação de tools.
- `src/widgets/`: componentes reutilizáveis.
- `src/theme/ThemeManager.*`: tema claro/escuro e QSS.
- `src/client/RealAtenaClient.*`: ponte UI -> SDK/Core. As operações ainda marcadas como `FEATURE_UNAVAILABLE` são o foco da integração de providers/modelos.
- `resources/icons/`: ícones SVG.
- `resources/images/`: ícone do aplicativo, marca e mascote.
- `resources/atena.qrc`: registro dos assets Qt.

## Build focado na UI

```bash
cmake -S . -B build-ui -DCMAKE_BUILD_TYPE=Debug -DATENA_BUILD_UI=ON -DATENA_BUILD_TESTS=OFF
cmake --build build-ui --target atena-ui -j2
./build-ui/ui/qt/atena-ui
```

O Core deve ser iniciado automaticamente pelo SDK através de `atena_client_connect_or_start()`.
