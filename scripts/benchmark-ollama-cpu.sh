#!/usr/bin/env bash
set -euo pipefail

MODEL="${1:-qwen3:0.6b}"
URL="${OLLAMA_HOST:-http://127.0.0.1:11434}"
PROMPT="${ATENA_BENCH_PROMPT:-Explique em uma frase o que é um algoritmo.}"
TIMEOUT="${ATENA_BENCH_TIMEOUT:-180}"
KEEP_ALIVE="${ATENA_BENCH_KEEP_ALIVE:-120}"

command -v curl >/dev/null || { echo "curl ausente"; exit 1; }
command -v jq >/dev/null || { echo "jq ausente"; exit 1; }
command -v awk >/dev/null || { echo "awk ausente"; exit 1; }

CPUS="$(getconf _NPROCESSORS_ONLN 2>/dev/null || nproc 2>/dev/null || echo 1)"
[[ "$CPUS" =~ ^[0-9]+$ ]] || CPUS=1
(( CPUS < 1 )) && CPUS=1

mem_avail_mib() {
  awk '/^MemAvailable:/ { printf "%d", $2/1024; found=1 } END { if (!found) print 0 }' /proc/meminfo 2>/dev/null || echo 0
}

AVAIL_MIB="$(mem_avail_mib)"
[[ "$AVAIL_MIB" =~ ^[0-9]+$ ]] || AVAIL_MIB=0

# Defaults aligned with Atena Runtime Manager for low-memory machines.
if [[ -n "${ATENA_BENCH_CTX:-}" ]]; then
  CTX="$ATENA_BENCH_CTX"
elif (( AVAIL_MIB > 0 && AVAIL_MIB < 1200 )); then
  CTX=2048
elif (( AVAIL_MIB > 0 && AVAIL_MIB < 2600 )); then
  CTX=3072
else
  CTX=4096
fi

if [[ -n "${ATENA_BENCH_BATCH:-}" ]]; then
  BATCH="$ATENA_BENCH_BATCH"
elif (( AVAIL_MIB > 0 && AVAIL_MIB < 1200 )); then
  BATCH=128
elif (( AVAIL_MIB > 0 && AVAIL_MIB < 2600 )); then
  BATCH=192
else
  BATCH=256
fi

if [[ -n "${ATENA_BENCH_THREADS:-}" ]]; then
  THREADS="$ATENA_BENCH_THREADS"
else
  if (( CPUS <= 2 )); then
    THREADS="1 2"
  elif (( CPUS <= 4 )); then
    THREADS="1 2 3 4"
  elif (( CPUS <= 8 )); then
    THREADS="2 4 6 $CPUS"
  else
    # Avoid testing impossible thread counts and reserve room for the OS.
    a=$(( CPUS / 2 ))
    b=$(( CPUS - 2 ))
    (( a < 2 )) && a=2
    (( b < 2 )) && b=2
    THREADS="2 4 $a $b $CPUS"
  fi
fi

# Deduplicate and discard values above the actual CPU count.
NORMALIZED=""
for t in $THREADS; do
  [[ "$t" =~ ^[0-9]+$ ]] || continue
  (( t >= 1 && t <= CPUS )) || continue
  case " $NORMALIZED " in
    *" $t "*) ;;
    *) NORMALIZED+=" $t" ;;
  esac
done
THREADS="${NORMALIZED# }"
[[ -n "$THREADS" ]] || THREADS="1"

echo "Atena Ollama CPU benchmark"
echo "Modelo: $MODEL | CPUs: $CPUS | RAM disponível: ${AVAIL_MIB} MiB"
echo "contexto: $CTX | batch: $BATCH | timeout/teste: ${TIMEOUT}s"
echo "threads candidatos: $THREADS"
echo

# Quick API check before a potentially long request.
if ! curl -fsS --max-time 5 "$URL/api/tags" >/dev/null; then
  echo "[erro] Ollama não responde em $URL"
  echo "Verifique: systemctl status ollama --no-pager"
  exit 1
fi

# Warm-up: model load is not useful when comparing num_thread.
echo "Aquecendo o modelo..."
warm_body="$(jq -nc --arg model "$MODEL" --argjson ctx "$CTX" --argjson batch "$BATCH" --argjson keep "$KEEP_ALIVE" \
  '{model:$model,stream:false,think:false,keep_alive:$keep,messages:[{role:"user",content:"Responda apenas: OK"}],options:{temperature:0,num_predict:4,num_ctx:$ctx,num_batch:$batch}}')"
if ! curl -fsS --max-time "$TIMEOUT" "$URL/api/chat" -d "$warm_body" >/dev/null; then
  echo "[erro] Falha ao aquecer $MODEL. Confirme com: ollama run $MODEL"
  exit 1
fi

echo
printf "%-8s %-12s %-12s %-12s\n" "threads" "tok/s" "eval_tokens" "eval_ms"
best_rate="0"
best_threads=""

for t in $THREADS; do
  echo "[teste] $t thread(s)..." >&2
  body="$(jq -nc --arg model "$MODEL" --arg prompt "$PROMPT" \
    --argjson threads "$t" --argjson ctx "$CTX" --argjson batch "$BATCH" --argjson keep "$KEEP_ALIVE" \
    '{model:$model,stream:false,think:false,keep_alive:$keep,messages:[{role:"user",content:$prompt}],options:{temperature:0,num_predict:64,num_thread:$threads,num_ctx:$ctx,num_batch:$batch}}')"

  if ! response="$(curl -fsS --max-time "$TIMEOUT" "$URL/api/chat" -d "$body")"; then
    printf "%-8s %-12s %-12s %-12s\n" "$t" "ERRO" "-" "-"
    continue
  fi

  count="$(jq -r '.eval_count // 0' <<<"$response")"
  dur="$(jq -r '.eval_duration // 0' <<<"$response")"
  rate="$(awk -v c="$count" -v d="$dur" 'BEGIN { if (d>0) printf "%.2f", c/(d/1000000000); else print "0.00" }')"
  eval_ms="$(awk -v d="$dur" 'BEGIN { printf "%.0f", d/1000000 }')"
  printf "%-8s %-12s %-12s %-12s\n" "$t" "$rate" "$count" "$eval_ms"

  better="$(awk -v a="$rate" -v b="$best_rate" 'BEGIN { print (a>b)?1:0 }')"
  if [[ "$better" == "1" ]]; then
    best_rate="$rate"
    best_threads="$t"
  fi
done

echo
if [[ -n "$best_threads" ]]; then
  echo "Melhor resultado: $best_threads thread(s) (${best_rate} tok/s)"
  echo "Teste no Atena: ATENA_OLLAMA_NUM_THREAD=$best_threads ./build/atena"
else
  echo "Nenhum teste completou. Verifique o Ollama e aumente ATENA_BENCH_TIMEOUT."
fi

echo "Observação: rode o benchmark com outros apps pesados fechados; ele próprio eleva o load average."
