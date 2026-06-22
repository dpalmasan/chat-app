#!/usr/bin/env bash
set -euo pipefail

# Fully reset local Kubernetes state for chat-app and redeploy from scratch.
# This deletes the minikube cluster, starts it again, and redeploys services.

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

require_cmd() {
  if ! command -v "$1" >/dev/null 2>&1; then
    echo "Error: required command '$1' is not installed."
    exit 1
  fi
}

require_cmd minikube
require_cmd kubectl
require_cmd docker

if [[ "${RESEED_DB:-0}" == "1" ]]; then
  if [[ -x "${ROOT_DIR}/scripts/reset_chat_db.sh" ]]; then
    echo "Reseeding local MongoDB before cluster reset..."
    "${ROOT_DIR}/scripts/reset_chat_db.sh"
  else
    echo "Warning: scripts/reset_chat_db.sh not found or not executable; skipping DB reseed."
  fi
fi

echo "Deleting Minikube cluster (profile: minikube)..."
minikube delete -p minikube || true

echo "Recreating cluster and deploying chat-app services..."
"${ROOT_DIR}/scripts/start_k8s_services.sh"

echo
echo "Full Kubernetes reset complete."
echo "Tip: use RESEED_DB=1 to reseed local MongoDB before recreating the cluster."
