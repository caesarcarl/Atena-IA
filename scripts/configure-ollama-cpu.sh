#!/usr/bin/env bash
set -euo pipefail

if ! command -v ollama >/dev/null 2>&1; then echo "[erro] Ollama não encontrado."; exit 1; fi
if ! command -v systemctl >/dev/null 2>&1; then echo "[erro] systemd não encontrado."; exit 1; fi

DROPIN_DIR="/etc/systemd/system/ollama.service.d"
DROPIN="$DROPIN_DIR/20-atena-cpu.conf"

echo "Ajuste CPU-only/baixa RAM: 1 requisição paralela, 1 modelo carregado, fila curta."
echo "O Atena continua definindo contexto, threads, batch e keep_alive por requisição."

sudo mkdir -p "$DROPIN_DIR"
sudo tee "$DROPIN" >/dev/null <<'EOF'
[Service]
Environment="OLLAMA_NUM_PARALLEL=1"
Environment="OLLAMA_MAX_LOADED_MODELS=1"
Environment="OLLAMA_MAX_QUEUE=32"
EOF

sudo systemctl daemon-reload
sudo systemctl restart ollama
sleep 1
systemctl --no-pager --full status ollama | sed -n '1,18p'
echo "OK: $DROPIN"
