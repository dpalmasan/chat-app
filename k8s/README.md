# Step 5 - Kubernetes + NGINX Ingress (Minikube)

This setup deploys all services into Minikube and exposes a single ingress
entrypoint (`http://chat.local`) for the web client and websocket traffic.

## 1) Start Minikube and enable ingress

```bash
minikube start
minikube addons enable ingress
```

## 2) Build images inside Minikube Docker daemon

```bash
eval "$(minikube -p minikube docker-env)"

docker build -t chat-app/chat-service:latest backend/services/chat_service
docker build -t chat-app/message-queue:latest backend/services/message_queue
docker build -t chat-app/db-service:latest backend/services/db_service
docker build -t chat-app/web-client:latest clients/web_client
```

## 3) Apply manifests

```bash
kubectl apply -f k8s/namespace.yaml
kubectl apply -f k8s/mongodb.yaml
kubectl apply -f k8s/db-service.yaml
kubectl apply -f k8s/message-queue.yaml
kubectl apply -f k8s/chat-service.yaml
kubectl apply -f k8s/web-client.yaml
kubectl apply -f k8s/ingress.yaml
```

## 4) Wire local host to ingress

Get Minikube IP:

```bash
minikube ip
```

Add to `/etc/hosts`:

```text
<MINIKUBE_IP> chat.local
```

## 5) Open app

Open:

```text
http://chat.local
```

The frontend is now glued to the ingress entrypoint:

- `/` -> web client
- `/api/*` -> web client Flask API
- `/ws` -> chat service websocket backend

## Useful checks

```bash
kubectl get pods -n chat-app
kubectl get svc -n chat-app
kubectl get ingress -n chat-app
kubectl logs -n chat-app deploy/web-client
kubectl logs -n chat-app deploy/chat-service
```
