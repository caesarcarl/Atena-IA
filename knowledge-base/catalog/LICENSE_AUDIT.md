# Auditoria inicial de fontes - Atena Knowledge Seed v0.1

Data da auditoria: 2026-10-01.

Esta lista e uma triagem tecnica inicial para staging do RAG, nao aconselhamento juridico. A regra do projeto e conservadora: uma fonte so entra automaticamente quando a licenca do material e os termos da fonte nao apresentam bloqueio conhecido ao uso pretendido. A atribuicao original e os avisos de licenca devem ser preservados.

## Liberadas para staging

- Python para Todos (PT-BR), Charles R. Severance: CC BY-NC-SA 3.0 no proprio PDF oficial. Fonte: https://www.py4e.com/book
- Apostila - Curso de Logica de Programacao, IFFluminense/eduCAPES: CC BY-NC 3.0 Brasil. Fonte: https://educapes.capes.gov.br/handle/capes/560827
- Primeiro Programa em Linguagem C, USP/eduCAPES: CC BY 3.0 Brasil. Fonte: https://educapes.capes.gov.br/handle/capes/597806
- Think C++, Allen B. Downey: CC BY-NC-SA 4.0. Fonte: https://greenteapress.com/wp/think-c/
- Algorithms, Jeff Erickson: livro sob CC BY 4.0. Fonte: https://jeffe.cs.illinois.edu/teaching/algorithms/
- Discrete Mathematics: An Open Introduction, 3rd ed., Oscar Levin: CC BY-SA 4.0. Fonte: https://discrete.openmathbooks.org/dmoi3.html
- Active Calculus: Single Variable, 2nd ed.: CC BY-SA 4.0. Fonte: https://scholarworks.gvsu.edu/books/34/
- Light and Matter, Benjamin Crowell: pagina do autor e item do Internet Archive indicam CC BY-SA; por cautela, o seed marca ingestao `text_only` por causa de creditos individuais de imagens/fotos.

## Quarentena

- OpenStax: nao automatizar ingestao. Paginas atuais de livros da OpenStax declaram que o conteudo nao pode ser usado em treinamento ou ser ingerido em LLMs/ofertas de IA generativa sem permissao previa por escrito. Uma licenca Creative Commons mostrada na pagina nao deve ser tratada isoladamente dessa condicao atual.
- OpenIntro: os livros em geral possuem licencas abertas, mas os Termos atuais do site incluem condicoes especiais para acesso automatizado/scraping. O seed nao raspa nem baixa OpenIntro automaticamente. Se for usado depois, auditar o arquivo especifico obtido legitimamente pelo usuario e sua licenca.

## Regra para novas fontes

Antes de adicionar uma nova fonte, registrar: URL oficial, autor, edicao/data, licenca exata, se e redistribuivel, se ingestao em IA e permitida, escopo de ingestao (texto/imagens), pack, trust tier e observacoes de atribuicao. Fonte sem licenca clara fica em quarentena.
