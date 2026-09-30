#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${ATENA_BUILD_DIR:-$ROOT/build-release}"
PREFIX="${ATENA_PREFIX:-/usr/local}"
JOBS="${ATENA_JOBS:-2}"
cmake -S "$ROOT" -B "$BUILD" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$PREFIX" \
  -DATENA_BUILD_UI=ON \
  -DATENA_REQUIRE_UI=ON \
  -DATENA_BUILD_TESTS=ON
cmake --build "$BUILD" -j"$JOBS"
ctest --test-dir "$BUILD" --output-on-failure
printf '\nBuild concluído. Para instalar em %s:\n  sudo cmake --install %q\n' "$PREFIX" "$BUILD"
