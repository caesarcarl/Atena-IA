# Atena Knowledge Programming v0.2

Base oficial de estudo de programacao/logica para o RAG do Atena.

Conteudo:
- Manual_Cognitivo_Atena_Logica_Algoritmos_Programacao.pdf
- identity_modules/programming.json
- baixar-livros-abertos.sh
- ingestir-no-atena.sh
- meta/sources.json

Uso no Debian 13:

  chmod +x baixar-livros-abertos.sh ingestir-no-atena.sh
  ./baixar-livros-abertos.sh "$HOME/Atena-Knowledge-Programming-v0.2"

Para incluir o manual na mesma pasta de PDFs:

  cp Manual_Cognitivo_Atena_Logica_Algoritmos_Programacao.pdf pdf/

Depois, a partir da raiz do Atena:

  ATENA_BIN=./build/atena /caminho/Atena-Knowledge-Programming-v0.2/ingestir-no-atena.sh /caminho/Atena-Knowledge-Programming-v0.2

Regra de produto:
O RAG apoia a resposta sem narrar o mecanismo de recuperacao por padrao. Fontes continuam disponiveis quando o usuario pede citacoes ou quando a proveniencia importa.

Licencas:
Cada livro tem uma licenca propria. Consulte meta/sources.json antes de redistribuir. O manual do Atena e CC BY 4.0.
