# RAG da Atena 0.5.4

O RAG continua pertencendo ao Core. O worker Python do desktop apenas extrai e prepara
fontes que o Core C/C++ indexa localmente.

## Desktop

```bash
./build/atena --demo
atena[mock]> /rag add /caminho/manual.pdf
atena[mock]> /rag list
```

PDF usa `pdftotext`/Poppler quando disponível. TXT, Markdown e código podem ser ingeridos
nativamente pela CLI.

## Mobile

Compile com:

```bash
cmake -S . -B build-mobile -DATENA_MOBILE=ON -DATENA_BUILD_UI=OFF
```

Nesse perfil o worker Python é desabilitado. O Core C/C++ continua consultando o RAG local.
A estratégia recomendada para PDF no mobile é indexar no desktop e depois copiar o índice/
pacote de conhecimento para o dispositivo. A exportação de ragpack será implementada em um
patch posterior.
