#!/usr/bin/env bash
set -Eeuo pipefail
trap 'rc=$?; echo "Falha na linha $LINENO (exit=$rc)." >&2; exit "$rc"' ERR
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
echo "=== ATENA 0.4.1 / gates P0 ==="

echo "[1/7] Auditoria de paths"
if grep -R -nE '"(\./)+atena\.db"|"\./identity"|"/tmp/atena-core\.sock"' core services sdk ipc platform 2>/dev/null; then
  echo "ERRO: defaults de produção dependentes de CWD ou socket global /tmp ainda existem." >&2
  exit 11
fi
echo "OK: data_dir + \"atena.db\" é permitido e não depende do CWD."

echo "[2/7] Testes não dependem de assert()"
if grep -R -nE '\bassert[[:space:]]*\(' tests --include='*.c' --include='*.cpp' 2>/dev/null; then
  echo "ERRO: assert() funcional ainda existe." >&2
  exit 12
fi

echo "[3/7] Qt QObject headers explicitamente no target"
python3 - <<'PY'
from pathlib import Path
q=Path("ui/qt")
cm=(q/"CMakeLists.txt").read_text()
missing=[]
for p in (q/"src").rglob("*.h"):
    if "Q_OBJECT" in p.read_text(errors="ignore") and p.relative_to(q).as_posix() not in cm:
        missing.append(str(p))
if missing:
    raise SystemExit("Q_OBJECT fora do target: "+", ".join(missing))
print("OK")
PY

echo "[4/7] Build Debug + testes"
rm -rf build-debug
cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug -DATENA_BUILD_TESTS=ON -DATENA_BUILD_UI=ON
cmake --build build-debug -j2
ctest --test-dir build-debug --output-on-failure

echo "[5/7] Build Release + testes"
rm -rf build-release
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release -DATENA_BUILD_TESTS=ON -DATENA_BUILD_UI=ON
cmake --build build-release -j2
ctest --test-dir build-release --output-on-failure

echo "[6/7] Gate do executável Qt"
if grep -q 'Qt6_FOUND' build-debug/CMakeCache.txt 2>/dev/null || [[ -x build-debug/ui/qt/atena-ui || -x build-debug/atena-ui ]]; then
  find build-debug -type f -name atena-ui -perm -111 -print -quit | grep -q . || { echo "Qt detectado mas atena-ui não linkou" >&2; exit 16; }
else
  echo "Qt não disponível neste host: gate Qt fica para host com Qt6-dev."
fi

echo "[7/7] Smoke do Core"
CORE="$(find build-debug -type f -name atena-core -perm -111 -print -quit)"
if [[ -z "$CORE" ]]; then
    echo "ERRO: atena-core não encontrado no build Debug." >&2
    exit 17
fi

echo "Core encontrado: $CORE"
SMOKE_DIR="$(mktemp -d)"
LOG="$SMOKE_DIR/core.log"
cleanup_smoke() { rm -rf "$SMOKE_DIR"; }
trap cleanup_smoke EXIT

mkdir -p "$SMOKE_DIR/config" "$SMOKE_DIR/data" "$SMOKE_DIR/cache" "$SMOKE_DIR/runtime"
chmod 700 "$SMOKE_DIR/config" "$SMOKE_DIR/data" "$SMOKE_DIR/cache" "$SMOKE_DIR/runtime"

echo "Executando Core em ambiente isolado..."
rc=0
if env \
    XDG_CONFIG_HOME="$SMOKE_DIR/config" \
    XDG_DATA_HOME="$SMOKE_DIR/data" \
    XDG_CACHE_HOME="$SMOKE_DIR/cache" \
    XDG_RUNTIME_DIR="$SMOKE_DIR/runtime" \
    ATENA_IDENTITY_DIR="$ROOT/identity" \
    ATENA_ENABLE_MOCK=1 \
    timeout 4s "$CORE" >"$LOG" 2>&1
then
    rc=0
else
    rc=$?
fi

echo
echo "===== LOG DO CORE ====="
[[ -f "$LOG" ]] && cat "$LOG" || true
echo "======================="
echo

case "$rc" in
    0)   echo "OK: Core iniciou e encerrou normalmente." ;;
    124) echo "OK: Core permaneceu ativo durante os 4 segundos do smoke test." ;;
    2)   echo "OK: uma instância do Core já estava ativa." ;;
    *)
        echo "ERRO: Core falhou no smoke test. exit=$rc" >&2
        exit "$rc"
        ;;
esac

echo
echo "============================================"
echo " ATENA 0.4.1: TODOS OS GATES P0 PASSARAM"
echo "============================================"
