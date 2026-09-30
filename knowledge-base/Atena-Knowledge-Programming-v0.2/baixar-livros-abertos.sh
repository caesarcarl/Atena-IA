#!/usr/bin/env bash
set -euo pipefail
ROOT="${1:-$HOME/Atena-Knowledge-Programming-v0.2}"
PDF_DIR="$ROOT/pdf"
mkdir -p "$PDF_DIR" "$ROOT/meta"
command -v curl >/dev/null || { echo "Instale curl: sudo apt install -y curl ca-certificates"; exit 1; }
command -v file >/dev/null || true

dl(){ name="$1"; url="$2"; echo "==> $name"; curl -fL --retry 3 --retry-delay 2 --connect-timeout 20 --progress-bar -C - "$url" -o "$PDF_DIR/$name"; [ -s "$PDF_DIR/$name" ] || { echo "arquivo vazio"; exit 2; }; }

dl "01_Logica_de_Programacao_IFFluminense.pdf" "https://educapes.capes.gov.br/bitstream/capes/560827/2/Apostila%20-%20Curso%20de%20L%C3%B3gica%20de%20Programa%C3%A7%C3%A3o.pdf"
dl "02_Python_para_Todos_PT-BR.pdf" "https://do1.dr-chuck.com/pythonlearn/PT_br/pythonlearn.pdf"
dl "03_Linguagem_C_Exercicios_Resolvidos.pdf" "https://repositorio.unb.br/bitstream/10482/25306/3/LIVRO_LinguagemCAprendendoExerciciosResolvidos.pdf"
dl "04_Think_CPP_Allen_Downey.pdf" "https://www.greenteapress.com/thinkcpp/thinkCScpp.pdf"
dl "05_Algorithms_Jeff_Erickson.pdf" "https://jeffe.cs.illinois.edu/teaching/algorithms/book/Algorithms-JeffE.pdf"
(cd "$PDF_DIR" && sha256sum ./*.pdf > "$ROOT/meta/SHA256SUMS")
echo "Biblioteca baixada: $PDF_DIR"
echo "Confira licencas em meta/sources.json antes de redistribuir."
