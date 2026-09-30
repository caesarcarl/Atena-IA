#!/usr/bin/env bash
set -Eeuo pipefail
project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
JOBS="${ATENA_JOBS:-2}"
if ! command -v apt-get >/dev/null 2>&1; then
  echo 'Este instalador requer uma distribuição baseada em Debian/Ubuntu/Mint.' >&2
  exit 2
fi
sudo apt-get update
sudo apt-get install -y build-essential cmake pkg-config qt6-base-dev qt6-svg-dev libsqlite3-dev libjson-c-dev libcurl4-openssl-dev libssl-dev ca-certificates
cmake -S "$project_dir" -B "$project_dir/build" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr -DATENA_BUILD_UI=ON -DATENA_BUILD_TESTS=ON
cmake --build "$project_dir/build" -j"$JOBS"
ctest --test-dir "$project_dir/build" --output-on-failure
sudo cmake --install "$project_dir/build"
echo 'Instalação concluída. Abra pelo menu “Atena” ou execute: atena-ui'
