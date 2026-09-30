#!/usr/bin/env bash
set -Eeuo pipefail
project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
JOBS="${ATENA_JOBS:-2}"
sudo apt-get update
sudo apt-get install -y \
  build-essential cmake pkg-config \
  libsqlite3-dev libjson-c-dev libcurl4-openssl-dev libssl-dev \
  qt6-base-dev qt6-svg-dev
cmake -S "${project_dir}" -B "${project_dir}/build" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/usr \
  -DATENA_BUILD_TESTS=ON \
  -DATENA_BUILD_UI=ON
cmake --build "${project_dir}/build" -j"${JOBS}"
ctest --test-dir "${project_dir}/build" --output-on-failure
sudo cmake --install "${project_dir}/build"
printf 'Atena 0.4.2 instalada. Execute: atena-ui\n'
if ! command -v ollama >/dev/null 2>&1; then
  printf 'Ollama não está instalado. Execute scripts/setup-ollama.sh qwen3:0.6b para instalar o runtime local.\n'
fi
