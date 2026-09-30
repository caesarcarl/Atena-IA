#!/usr/bin/env bash
set -Eeuo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
JOBS="${ATENA_BUILD_JOBS:-$(nproc)}"

if ! command -v sudo >/dev/null 2>&1; then
  echo "[ERRO] sudo não encontrado." >&2
  exit 1
fi

. /etc/os-release 2>/dev/null || true
case "${ID:-unknown}:${ID_LIKE:-}" in
  linuxmint:*|ubuntu:*|*:ubuntu*|*:debian*) ;;
  *) echo "[AVISO] Ambiente não identificado como Mint/Ubuntu/Debian; continuando." >&2 ;;
esac

echo "[1/5] Atualizando APT..."
sudo apt-get update

echo "[2/5] Instalando toolchain e dependências desktop..."
sudo DEBIAN_FRONTEND=noninteractive apt-get install -y \
  build-essential gcc g++ cmake ninja-build pkg-config git \
  curl wget ca-certificates unzip zip jq file \
  sqlite3 libsqlite3-dev libjson-c-dev \
  libcurl4-openssl-dev libssl-dev libsecret-1-dev \
  python3 python3-venv python3-pip poppler-utils \
  gdb strace lsof

echo "[3/5] Limpando build antigo..."
rm -rf "$ROOT/build"

echo "[4/5] Compilando Atena (${JOBS} jobs)..."
cmake -S "$ROOT" -B "$ROOT/build" -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DATENA_BUILD_UI=OFF \
  -DATENA_ENABLE_PYTHON_WORKER=ON
cmake --build "$ROOT/build" -j"$JOBS"

echo "[5/5] Rodando testes..."
ctest --test-dir "$ROOT/build" --output-on-failure

cat <<MSG

============================================================
ATENA 0.5.6 - Mint/Debian bootstrap concluído
============================================================
CLI:  $ROOT/build/atena
Core: $ROOT/build/atena-core

Teste seguro:
  cd "$ROOT"
  ./build/atena --demo

Ollama real:
  ./build/atena

Diagnóstico:
  /doctor
  /runtime
  /identity
  /providers
  /models

RAG:
  /rag add /caminho/arquivo.pdf
  /rag list

Benchmark CPU/Ollama:
  ./scripts/benchmark-ollama-cpu.sh qwen3:0.6b

Ajuste de serviço Ollama para máquina CPU-only:
  ./scripts/configure-ollama-cpu.sh
============================================================
MSG
