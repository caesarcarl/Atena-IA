#!/usr/bin/env bash
set -Eeuo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${ATENA_BUILD_DIR:-$ROOT/build-0.6.0}"
PACKAGE=0
[[ "${1:-}" == "--package" ]] && PACKAGE=1

command -v cmake >/dev/null || { echo "cmake ausente" >&2; exit 1; }
command -v ninja >/dev/null || { echo "ninja ausente" >&2; exit 1; }

cmake -S "$ROOT" -B "$BUILD" -G Ninja \
  -DATENA_BUILD_TESTS=ON \
  -DATENA_BUILD_UI=ON \
  -DATENA_REQUIRE_UI=ON \
  -DATENA_PACKAGE_REVISION=0.6.0-knowledge-rag

cmake --build "$BUILD" -j"$(nproc)"
ctest --test-dir "$BUILD" --output-on-failure

echo
echo "=== Atena 0.6.0 build OK ==="
"$BUILD/atena" status || true

if [[ -f "$ROOT/knowledge-base/prebuilt/knowledge.sqlite" ]]; then
  echo
echo "=== Knowledge smoke ==="
  ATENA_KNOWLEDGE_DB="$ROOT/knowledge-base/prebuilt/knowledge.sqlite" \
  ATENA_RAG_FORCE=1 ATENA_OLLAMA_THINK=off ATENA_OLLAMA_NUM_CTX=2048 \
    "$BUILD/atena" run qwen3:0.6b "Explique em poucas linhas o que é busca em largura e por que ela usa uma fila." || true
fi

if [[ "$PACKAGE" -eq 1 ]]; then
  (cd "$BUILD" && cpack -G DEB)
  echo "Pacote(s):"
  find "$BUILD" -maxdepth 1 -type f -name '*.deb' -print
fi
