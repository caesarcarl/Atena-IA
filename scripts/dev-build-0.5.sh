#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${ATENA_BUILD_DIR:-$ROOT/build}"
GENERATOR=()
if command -v ninja >/dev/null 2>&1; then GENERATOR=(-G Ninja); fi
cmake -S "$ROOT" -B "$BUILD" "${GENERATOR[@]}" \
  -DATENA_BUILD_UI="${ATENA_BUILD_UI:-OFF}" \
  -DATENA_BUILD_TESTS=ON \
  -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE:-RelWithDebInfo}"
cmake --build "$BUILD" -j"${ATENA_JOBS:-2}"
ctest --test-dir "$BUILD" --output-on-failure
printf '\nOK: %s/atena\n' "$BUILD"
