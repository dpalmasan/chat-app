#!/usr/bin/env bash
set -euo pipefail

# Tear down chat-app resources from Minikube and stop the local cluster.

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
NAMESPACE="chat-app"

require_cmd() {
  if ! command -v "$1" >/dev/null 2>&1; then
    echo "Error: required command '$1' is not installed."
    exit 1
  fi
}

require_cmd minikube
require_cmd kubectl

if ! minikube status >/dev/null 2>&1; then
  echo "Minikube is not running."
  exit 0
fi

echo "Cleaning Kubernetes resources in namespace '${NAMESPACE}'..."
if kubectl get namespace "${NAMESPACE}" >/dev/null 2>&1; then
  kubectl delete namespace "${NAMESPACE}" --wait=true --timeout=180s
else
  echo "Namespace '${NAMESPACE}' not found. Nothing to delete."
fi

if [[ "${CLEAN_IMAGES:-0}" == "1" ]]; then
  require_cmd docker
  echo "Removing local Minikube images for chat-app..."
  eval "$(minikube -p minikube docker-env)"
  docker image rm \
    chat-app/chat-service:latest \
    chat-app/message-queue:latest \
    chat-app/db-service:latest \
    chat-app/web-client:latest || true
fi

echo "Stopping Minikube..."
minikube stop

echo
echo "Kubernetes teardown complete."
echo "Tip: set CLEAN_IMAGES=1 to also remove built images in Minikube Docker."
