#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
JOBS="${ATENA_BUILD_JOBS:-$(nproc)}"

if ! command -v sudo >/dev/null 2>&1; then
  echo "[ERRO] sudo não encontrado. Entre como root ou instale/configure sudo." >&2
  exit 1
fi

if ! grep -qiE 'debian|trixie' /etc/os-release 2>/dev/null; then
  echo "[AVISO] Este bootstrap foi preparado para Debian 13/trixie; continuando mesmo assim." >&2
fi

echo "[1/5] Atualizando índice APT..."
sudo apt-get update

echo "[2/5] Instalando toolchain e dependências do Atena..."
sudo DEBIAN_FRONTEND=noninteractive apt-get install -y \
  build-essential gcc g++ cmake ninja-build pkg-config git \
  curl wget ca-certificates unzip zip jq \
  sqlite3 libsqlite3-dev libjson-c-dev \
  libcurl4-openssl-dev libssl-dev libsecret-1-dev \
  python3 python3-venv python3-pip poppler-utils \
  gdb strace lsof

echo "[3/5] Limpando build anterior..."
rm -rf "$ROOT/build"

echo "[4/5] Configurando e compilando (${JOBS} jobs)..."
cmake -S "$ROOT" -B "$ROOT/build" -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DATENA_BUILD_UI=OFF \
  -DATENA_ENABLE_PYTHON_WORKER=ON
cmake --build "$ROOT/build" -j"$JOBS"

echo "[5/5] Executando testes..."
ctest --test-dir "$ROOT/build" --output-on-failure

cat <<MSG

============================================================
ATENA - bootstrap Debian concluído
============================================================
Binários:
  $ROOT/build/atena
  $ROOT/build/atena-core

Teste seguro com provider mock:
  cd "$ROOT"
  ./build/atena --demo

Diagnóstico:
  ./build/atena --demo
  /doctor
  /runtime
  /identity

Para PDF/RAG no desktop:
  /rag add /caminho/arquivo.pdf
  /rag list

Para Ollama real, instale/inicie o Ollama e depois rode:
  ./build/atena

O Core não precisa do worker Python para iniciar. Python/Poppler são
usados como camada desktop opcional para ingestão de PDF e tarefas pesadas.
============================================================
MSG
