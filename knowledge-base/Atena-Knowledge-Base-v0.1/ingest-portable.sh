#!/usr/bin/env bash
set -euo pipefail
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ATENA_BIN="${ATENA_BIN:-./build/atena}"
for file in "$DIR"/txt/*.txt; do
  echo "==> $file"
  "$ATENA_BIN" rag add "$file"
done
"$ATENA_BIN" rag list
