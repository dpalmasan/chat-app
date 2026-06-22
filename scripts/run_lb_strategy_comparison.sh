#!/usr/bin/env bash
set -euo pipefail

# Run load test for multiple ingress balancing strategies and compare outputs.

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
NAMESPACE="chat-app"
INGRESS_NAME="chat-app-ingress"

STRATEGIES="${STRATEGIES:-round_robin,hash_client_id,ewma}"
DURATION_SECONDS="${DURATION_SECONDS:-60}"
CLIENTS="${CLIENTS:-20}"
SAMPLE_SECONDS="${SAMPLE_SECONDS:-2}"
SEND_INTERVAL_SECONDS="${SEND_INTERVAL_SECONDS:-0.7}"
ANIMATE="${ANIMATE:-1}"
ANIMATION_FORMAT="${ANIMATION_FORMAT:-gif}"
ANIMATION_FPS="${ANIMATION_FPS:-6}"
WS_ENDPOINT="${WS_ENDPOINT:-ws://chat.local/ws}"
LOAD_USERS="${LOAD_USERS:-user_a,user_b,user_c,user_d,user_e}"

RUN_TS="$(date -u +%Y%m%dT%H%M%SZ)"
OUTPUT_ROOT="${OUTPUT_ROOT:-${ROOT_DIR}/artifacts/lb_strategy_${RUN_TS}}"
mkdir -p "${OUTPUT_ROOT}"

require_cmd() {
  if ! command -v "$1" >/dev/null 2>&1; then
    echo "Error: required command '$1' is not installed."
    exit 1
  fi
}

require_cmd kubectl
require_cmd python3

if ! kubectl get namespace "${NAMESPACE}" >/dev/null 2>&1; then
  echo "Error: namespace '${NAMESPACE}' not found. Start k8s services first."
  exit 1
fi

apply_strategy() {
  local strategy="$1"
  echo "Applying strategy: ${strategy}"

  case "${strategy}" in
    round_robin)
      kubectl annotate ingress "${INGRESS_NAME}" -n "${NAMESPACE}" \
        nginx.ingress.kubernetes.io/upstream-hash-by- --overwrite >/dev/null
      kubectl annotate ingress "${INGRESS_NAME}" -n "${NAMESPACE}" \
        nginx.ingress.kubernetes.io/load-balance=round_robin --overwrite >/dev/null
      ;;
    hash_client_id)
      kubectl annotate ingress "${INGRESS_NAME}" -n "${NAMESPACE}" \
        nginx.ingress.kubernetes.io/load-balance=round_robin --overwrite >/dev/null
      kubectl annotate ingress "${INGRESS_NAME}" -n "${NAMESPACE}" \
        nginx.ingress.kubernetes.io/upstream-hash-by='$arg_client_id' --overwrite >/dev/null
      ;;
    ewma)
      kubectl annotate ingress "${INGRESS_NAME}" -n "${NAMESPACE}" \
        nginx.ingress.kubernetes.io/upstream-hash-by- --overwrite >/dev/null
      kubectl annotate ingress "${INGRESS_NAME}" -n "${NAMESPACE}" \
        nginx.ingress.kubernetes.io/load-balance=ewma --overwrite >/dev/null
      ;;
    *)
      echo "Error: unknown strategy '${strategy}'"
      exit 1
      ;;
  esac

  kubectl get ingress "${INGRESS_NAME}" -n "${NAMESPACE}" >/dev/null
}

IFS=',' read -r -a strategy_list <<< "${STRATEGIES}"

for strategy in "${strategy_list[@]}"; do
  strategy="$(echo "${strategy}" | xargs)"
  [[ -z "${strategy}" ]] && continue

  apply_strategy "${strategy}"

  out_dir="${OUTPUT_ROOT}/${strategy}"
  echo "Running load test for ${strategy}..."
  OUTPUT_DIR="${out_dir}" \
  DURATION_SECONDS="${DURATION_SECONDS}" \
  CLIENTS="${CLIENTS}" \
  SAMPLE_SECONDS="${SAMPLE_SECONDS}" \
  SEND_INTERVAL_SECONDS="${SEND_INTERVAL_SECONDS}" \
  ANIMATE="${ANIMATE}" \
  ANIMATION_FORMAT="${ANIMATION_FORMAT}" \
  ANIMATION_FPS="${ANIMATION_FPS}" \
  WS_ENDPOINT="${WS_ENDPOINT}" \
  LOAD_USERS="${LOAD_USERS}" \
  POD_TOUCH_SOURCE=ingress \
  "${ROOT_DIR}/scripts/run_load_test_with_charts.sh"
done

echo "Generating cross-strategy comparison charts..."
python3 "${ROOT_DIR}/scripts/plot_lb_strategy_comparison.py" --root-dir "${OUTPUT_ROOT}"

echo
echo "Strategy comparison complete. Artifacts: ${OUTPUT_ROOT}"
echo "- ${OUTPUT_ROOT}/strategy_pod_touch_comparison.png"
echo "- ${OUTPUT_ROOT}/strategy_db_growth_comparison.png"
