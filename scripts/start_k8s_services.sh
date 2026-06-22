#!/usr/bin/env bash
set -euo pipefail

# Start local Kubernetes stack in Minikube and deploy chat-app services.

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
require_cmd docker

if ! minikube status >/dev/null 2>&1; then
  echo "Starting minikube..."
  minikube start --driver=docker
else
  echo "Minikube already running."
fi

echo "Enabling ingress addon..."
minikube addons enable ingress

echo "Using minikube docker daemon for image builds..."
eval "$(minikube -p minikube docker-env)"

echo "Building service images..."
docker build -t chat-app/chat-service:latest -f "${ROOT_DIR}/backend/services/chat_service/Dockerfile" "${ROOT_DIR}"
docker build -t chat-app/message-queue:latest -f "${ROOT_DIR}/backend/services/message_queue/Dockerfile" "${ROOT_DIR}"
docker build -t chat-app/db-service:latest -f "${ROOT_DIR}/backend/services/db_service/Dockerfile" "${ROOT_DIR}"
docker build -t chat-app/web-client:latest -f "${ROOT_DIR}/clients/web_client/Dockerfile" "${ROOT_DIR}"

echo "Applying Kubernetes manifests..."
kubectl apply -f "${ROOT_DIR}/k8s/namespace.yaml"
kubectl apply -f "${ROOT_DIR}/k8s/mongodb.yaml"
kubectl apply -f "${ROOT_DIR}/k8s/db-service.yaml"
kubectl apply -f "${ROOT_DIR}/k8s/message-queue.yaml"
kubectl apply -f "${ROOT_DIR}/k8s/chat-service.yaml"
kubectl apply -f "${ROOT_DIR}/k8s/web-client.yaml"
kubectl apply -f "${ROOT_DIR}/k8s/ingress.yaml"

echo "Waiting for deployments..."
kubectl rollout status deployment/mongodb -n "${NAMESPACE}" --timeout=180s
kubectl rollout status deployment/db-service -n "${NAMESPACE}" --timeout=180s
kubectl rollout status deployment/message-queue -n "${NAMESPACE}" --timeout=180s
kubectl rollout status deployment/chat-service -n "${NAMESPACE}" --timeout=180s
kubectl rollout status deployment/web-client -n "${NAMESPACE}" --timeout=180s

echo "Current resources:"
kubectl get pods -n "${NAMESPACE}"
kubectl get svc -n "${NAMESPACE}"
kubectl get ingress -n "${NAMESPACE}"

MINIKUBE_IP="$(minikube ip)"

echo
echo "Deployment complete."
echo "Add this to /etc/hosts (if not present):"
echo "${MINIKUBE_IP} chat.local"
echo "Then open: http://chat.local"
