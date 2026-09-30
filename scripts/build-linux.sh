#!/usr/bin/env bash
set -Eeuo pipefail
project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${project_dir}/build"
JOBS="${ATENA_JOBS:-2}"
cmake -S "${project_dir}" -B "${build_dir}" -DCMAKE_BUILD_TYPE=Release -DATENA_BUILD_TESTS=ON -DATENA_BUILD_UI=ON
cmake --build "${build_dir}" -j"${JOBS}"
ctest --test-dir "${build_dir}" --output-on-failure
printf 'Build concluída em %s\n' "${build_dir}"
