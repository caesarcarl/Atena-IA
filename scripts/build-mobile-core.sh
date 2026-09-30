#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${ATENA_BUILD_DIR:-$ROOT/build-mobile}"
JOBS="${ATENA_JOBS:-2}"
cmake -S "$ROOT" -B "$BUILD" \
  -DCMAKE_BUILD_TYPE=Release \
  -DATENA_MOBILE=ON \
  -DATENA_ENABLE_PYTHON_WORKER=OFF \
  -DATENA_BUILD_UI=OFF \
  -DATENA_BUILD_TESTS=OFF
cmake --build "$BUILD" -j"$JOBS" --target atena atena-core
printf 'Mobile core pronto em %s\n' "$BUILD"
