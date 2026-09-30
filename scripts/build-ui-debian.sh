#!/usr/bin/env bash
set -Eeuo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${1:-$ROOT/build-ui}"
JOBS="${ATENA_JOBS:-2}"

cmake -S "$ROOT" -B "$BUILD" \
  -DCMAKE_BUILD_TYPE=Debug \
  -DATENA_BUILD_UI=ON \
  -DATENA_BUILD_TESTS=OFF

cmake --build "$BUILD" --target atena-ui -j"$JOBS"

echo
echo "UI pronta: $BUILD/ui/qt/atena-ui"
