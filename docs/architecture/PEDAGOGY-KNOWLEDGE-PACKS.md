# Atena: pedagogia executavel e Knowledge Packs

## Principio

`SAPERE AUDE` e um principio de ensino, nao uma obrigacao de transformar toda
resposta em interrogatorio. A Atena deve ajudar primeiro e aumentar a autonomia
do usuario sempre que isso for util.

## Modos pedagogicos

- `direct`: resposta/tarefa primeiro; nenhuma pergunta pedagogica obrigatoria.
- `adaptive`: padrao; ajuda imediatamente e explica conceitos e passos
  verificaveis quando isso melhora a compreensao.
- `study`: diagnostico breve, uma pergunta orientadora ou pista por turno,
  explicacao e transferencia para um novo exemplo.

O usuario pode sempre pedir uma resposta direta.

## Conhecimento nao e identidade

Knowledge Packs definem colecoes recuperaveis pelo RAG. Eles nao alteram a
personalidade da Atena.

- `atena.common`: nucleo compartilhado;
- `atena.dev`: programacao, computacao, Linux e engenharia;
- `atena.study`: educacao, vestibulares e exercicios;
- `atena.everyday`: casa, escritorio e tarefas digitais.

O perfil de demonstracao instala todos.

## Pipeline planejado

Documento -> extracao -> paginas/secoes -> chunks -> metadados -> FTS5 ->
embeddings -> recuperacao hibrida -> reranking -> contexto.

Um embedding nao substitui o documento. Cada chunk deve preservar origem,
locator/pagina, hash, pack e licenca.

## Proxima etapa

Separar o armazenamento RAG do runtime `:memory:` e tornar o indice
reconstruivel a partir das fontes originais.
