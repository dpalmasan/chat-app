#!/usr/bin/env bash
set -euo pipefail

# Run websocket load, sample pod/db metrics, and generate charts.

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
NAMESPACE="chat-app"
CHAT_LABEL="app=chat-service"
MONGO_DEPLOY="mongodb"
DB_NAME="${DB_NAME:-chat_app}"
POD_TOUCH_SOURCE="${POD_TOUCH_SOURCE:-ingress}"
INGRESS_NAMESPACE="${INGRESS_NAMESPACE:-ingress-nginx}"
INGRESS_CONTROLLER_DEPLOYMENT="${INGRESS_CONTROLLER_DEPLOYMENT:-ingress-nginx-controller}"
INGRESS_CONTROLLER_LABEL="${INGRESS_CONTROLLER_LABEL:-app.kubernetes.io/component=controller}"
CHAT_SERVICE_PORT="${CHAT_SERVICE_PORT:-8080}"

DURATION_SECONDS="${DURATION_SECONDS:-90}"
SAMPLE_SECONDS="${SAMPLE_SECONDS:-2}"
CLIENTS="${CLIENTS:-25}"
SEND_INTERVAL_SECONDS="${SEND_INTERVAL_SECONDS:-0.7}"
WS_ENDPOINT="${WS_ENDPOINT:-ws://chat.local/ws}"
LOAD_USERS="${LOAD_USERS:-user_a,user_b,user_c,user_d,user_e}"
ANIMATE="${ANIMATE:-0}"
ANIMATION_FORMAT="${ANIMATION_FORMAT:-gif}"
ANIMATION_FPS="${ANIMATION_FPS:-4}"

TIMESTAMP="$(date -u +%Y%m%dT%H%M%SZ)"
OUTPUT_DIR="${OUTPUT_DIR:-${ROOT_DIR}/artifacts/load_test_${TIMESTAMP}}"
mkdir -p "${OUTPUT_DIR}"

POD_CSV="${OUTPUT_DIR}/pod_touch_samples.csv"
DB_CSV="${OUTPUT_DIR}/db_population_samples.csv"
SUMMARY_JSON="${OUTPUT_DIR}/load_summary.json"

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

echo "timestamp,pod,handshake_count" >"${POD_CSV}"
echo "timestamp,messages_count,users_count" >"${DB_CSV}"

START_ISO="$(date -u +%Y-%m-%dT%H:%M:%SZ)"

sample_once() {
  local now_iso pod_ip_lines pod pod_ip logs upstream_ips ip mapped_pod
  local mongo_json msg_count users_count
  declare -A ip_to_pod
  declare -A pod_counts
  now_iso="$(date -u +%Y-%m-%dT%H:%M:%SZ)"

  mapfile -t pod_ip_lines < <(kubectl get pods -n "${NAMESPACE}" -l "${CHAT_LABEL}" -o jsonpath='{range .items[*]}{.metadata.name}{","}{.status.podIP}{"\n"}{end}')

  for line in "${pod_ip_lines[@]}"; do
    pod="${line%%,*}"
    pod_ip="${line#*,}"
    if [[ -z "${pod}" || -z "${pod_ip}" ]]; then
      continue
    fi
    ip_to_pod["${pod_ip}"]="${pod}"
    pod_counts["${pod}"]=0
  done

  if [[ "${POD_TOUCH_SOURCE}" == "ingress" ]]; then
    logs="$(kubectl logs -n "${INGRESS_NAMESPACE}" -l "${INGRESS_CONTROLLER_LABEL}" --since-time="${START_ISO}" --prefix=true 2>/dev/null || true)"
    upstream_ips="$(printf "%s\n" "${logs}" | grep 'GET /ws' | sed -n "s/.*] \[\] \([0-9.]\+\):${CHAT_SERVICE_PORT} .*/\1/p" || true)"
  else
    logs="$(kubectl logs -n "${NAMESPACE}" -l "${CHAT_LABEL}" --since-time="${START_ISO}" --prefix=true 2>/dev/null || true)"
    upstream_ips="$(printf "%s\n" "${logs}" | sed -n 's/.*pod\/\([^/]*\)\/chat-service].*Handshake complete.*/\1/p' || true)"
  fi

  while IFS= read -r ip; do
    [[ -z "${ip}" ]] && continue
    if [[ "${POD_TOUCH_SOURCE}" == "ingress" ]]; then
      mapped_pod="${ip_to_pod[${ip}]:-}"
    else
      mapped_pod="${ip}"
    fi
    if [[ -n "${mapped_pod}" ]]; then
      pod_counts["${mapped_pod}"]=$((pod_counts["${mapped_pod}"] + 1))
    fi
  done <<< "${upstream_ips}"

  for line in "${pod_ip_lines[@]}"; do
    pod="${line%%,*}"
    if [[ -z "${pod}" ]]; then
      continue
    fi
    echo "${now_iso},${pod},${pod_counts[${pod}]:-0}" >>"${POD_CSV}"
  done

  mongo_json="$(kubectl exec -n "${NAMESPACE}" deploy/${MONGO_DEPLOY} -- \
    mongosh --quiet --eval "const d=db.getSiblingDB('${DB_NAME}'); print(JSON.stringify({messages:d.messages.countDocuments({}),users:d.users.countDocuments({})}))" \
    2>/dev/null || true)"

  msg_count="$(printf "%s" "${mongo_json}" | sed -n 's/.*"messages"[[:space:]]*:[[:space:]]*\([0-9][0-9]*\).*/\1/p')"
  users_count="$(printf "%s" "${mongo_json}" | sed -n 's/.*"users"[[:space:]]*:[[:space:]]*\([0-9][0-9]*\).*/\1/p')"

  msg_count="${msg_count:-0}"
  users_count="${users_count:-0}"
  echo "${now_iso},${msg_count},${users_count}" >>"${DB_CSV}"
}

echo "Starting websocket load test..."
python3 "${ROOT_DIR}/scripts/ws_load_generator.py" \
  --endpoint "${WS_ENDPOINT}" \
  --clients "${CLIENTS}" \
  --duration "${DURATION_SECONDS}" \
  --send-interval "${SEND_INTERVAL_SECONDS}" \
  --users "${LOAD_USERS}" >"${SUMMARY_JSON}" &
LOAD_PID=$!

# Sample while load runs.
while kill -0 "${LOAD_PID}" >/dev/null 2>&1; do
  sample_once
  sleep "${SAMPLE_SECONDS}"
done

wait "${LOAD_PID}"

# Capture one last sample after connections close so ws access logs are reflected.
sample_once

echo "Rendering charts..."
python3 "${ROOT_DIR}/scripts/plot_load_test.py" --input-dir "${OUTPUT_DIR}"

if [[ "${ANIMATE}" == "1" ]]; then
  echo "Rendering animated chart(s)..."
  python3 "${ROOT_DIR}/scripts/animate_load_test.py" \
    --input-dir "${OUTPUT_DIR}" \
    --format "${ANIMATION_FORMAT}" \
    --fps "${ANIMATION_FPS}"
fi

echo
echo "Load test complete. Artifacts: ${OUTPUT_DIR}"
echo "- ${SUMMARY_JSON}"
echo "- ${POD_CSV}"
echo "- ${DB_CSV}"
echo "- ${OUTPUT_DIR}/pod_touch_chart.png"
echo "- ${OUTPUT_DIR}/db_population_chart.png"
if [[ "${ANIMATE}" == "1" ]]; then
  echo "- ${OUTPUT_DIR}/pod_touch_evolution.${ANIMATION_FORMAT}"
  echo "- ${OUTPUT_DIR}/db_population_evolution.${ANIMATION_FORMAT}"
fi
