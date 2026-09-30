#!/usr/bin/env bash
set -euo pipefail
ATENA_BIN="${ATENA_BIN:-./build/atena}"
BASE="${1:-$HOME/Atena-Knowledge-Programming-v0.2}"
PDF_DIR="$BASE/pdf"
[ -x "$ATENA_BIN" ] || { echo "Atena nao encontrada em $ATENA_BIN"; exit 1; }
mapfile -t files < <(find "$PDF_DIR" -maxdepth 1 -type f -name '*.pdf' | sort)
[ ${#files[@]} -gt 0 ] || { echo "Nenhum PDF em $PDF_DIR"; exit 1; }
tmp=$(mktemp); trap 'rm -f "$tmp"' EXIT
for f in "${files[@]}"; do printf '/rag add %s
' "$f" >> "$tmp"; done
printf '/rag list
/exit
' >> "$tmp"
"$ATENA_BIN" < "$tmp"
