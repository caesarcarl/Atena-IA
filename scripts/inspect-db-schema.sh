#!/usr/bin/env bash
set -euo pipefail
DB="${1:-${XDG_DATA_HOME:-$HOME/.local/share}/atena/atena.db}"
if ! command -v sqlite3 >/dev/null 2>&1; then
  echo "sqlite3 não está instalado" >&2
  exit 1
fi
if [[ ! -f "$DB" ]]; then
  echo "Banco não encontrado: $DB" >&2
  exit 1
fi
echo "database=$DB"
echo "--- quick_check ---"
sqlite3 "$DB" 'PRAGMA quick_check;'
echo "--- user_version ---"
sqlite3 "$DB" 'PRAGMA user_version;'
for t in sessions messages preferences providers operations audit_events documents chunks; do
  echo "--- $t ---"
  sqlite3 "$DB" "PRAGMA table_info('$t');" || true
done
