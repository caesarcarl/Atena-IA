#!/usr/bin/env bash
set -Eeuo pipefail
if command -v ollama >/dev/null 2>&1; then
  printf 'Ollama já está instalado: %s\n' "$(ollama --version 2>/dev/null || true)"
else
  installer="$(mktemp)"
  trap 'rm -f -- "${installer}"' EXIT
  curl --proto '=https' --tlsv1.2 -fsSL https://ollama.com/install.sh -o "${installer}"
  sh "${installer}"
fi
model="${1:-qwen3:0.6b}"
ollama pull "${model}"
printf 'Modelo %s pronto. Use: atena run %s\n' "${model}" "${model}"
