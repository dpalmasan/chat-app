#!/usr/bin/env bash
set -euo pipefail

# Install kubectl and minikube on Linux.
# This script installs binaries to /usr/local/bin.

if [[ "${EUID}" -ne 0 ]]; then
  echo "Please run as root (use sudo)."
  exit 1
fi

ARCH="amd64"
KUBECTL_STABLE="$(curl -fsSL https://dl.k8s.io/release/stable.txt)"

echo "Installing kubectl ${KUBECTL_STABLE}..."
curl -fsSL -o /tmp/kubectl "https://dl.k8s.io/release/${KUBECTL_STABLE}/bin/linux/${ARCH}/kubectl"
install -m 0755 /tmp/kubectl /usr/local/bin/kubectl

# Optional checksum verification.
curl -fsSL -o /tmp/kubectl.sha256 "https://dl.k8s.io/release/${KUBECTL_STABLE}/bin/linux/${ARCH}/kubectl.sha256"
echo "$(cat /tmp/kubectl.sha256)  /usr/local/bin/kubectl" | sha256sum --check

echo "Installing minikube latest..."
curl -fsSL -o /tmp/minikube-linux-amd64 "https://storage.googleapis.com/minikube/releases/latest/minikube-linux-amd64"
install -m 0755 /tmp/minikube-linux-amd64 /usr/local/bin/minikube

echo "kubectl version:"
kubectl version --client --output=yaml | head -n 20

echo "minikube version:"
minikube version

echo "Done."
