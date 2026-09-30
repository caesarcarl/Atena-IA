#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORKER="$ROOT/python/atena_worker"
printf '{"op":"health"}\n{"op":"chunk","text":"Atena worker teste de chunking."}\n' | PYTHONPATH="$WORKER" python3 -m atena_worker
