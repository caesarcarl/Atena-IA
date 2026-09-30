# Atena 0.5.4 - RAG + identidade + worker desktop

- adiciona `/identity` para inspecionar a identidade efetiva carregada pelo Core;
- adiciona `mission.json` à identidade Atena;
- adiciona `/rag add CAMINHO` e `/rag list` à CLI persistente;
- TXT/Markdown/código podem ser ingeridos pela CLI sem Python;
- PDF usa worker Python opcional e Poppler `pdftotext`, com fallback opcional para `pypdf`;
- o worker agrupa páginas para manter frames IPC pequenos;
- o `.deb` desktop inclui o código do worker e depende de `python3` + `poppler-utils`;
- `ATENA_MOBILE=ON` desliga o worker Python e mantém Core/CLI C/C++;
- o RAG continua armazenado e consultado localmente pelo Core;
- identidade e RAG continuam independentes do provider/LLM escolhido.
