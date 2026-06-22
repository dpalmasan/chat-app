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

## Load test with pod/db charts

This repo includes a scripted load test that:

- generates websocket traffic through ingress (`ws://chat.local/ws`)
- samples which `chat-service` pod handled handshakes over time
- samples MongoDB document growth (`messages` and `users`)
- renders PNG charts

### Prerequisites

- Kubernetes stack is running (`./scripts/start_k8s_services.sh`)
- Python packages:

```bash
pip install websockets matplotlib
```

### Run

```bash
./scripts/run_load_test_with_charts.sh
```

Optional tuning:

```bash
CLIENTS=50 DURATION_SECONDS=180 SAMPLE_SECONDS=2 ./scripts/run_load_test_with_charts.sh
```

Generate animations (GIF by default):

```bash
ANIMATE=1 ./scripts/run_load_test_with_charts.sh
```

Customize format/fps:

```bash
ANIMATE=1 ANIMATION_FORMAT=gif ANIMATION_FPS=6 ./scripts/run_load_test_with_charts.sh
```

`ANIMATION_FORMAT=mp4` is also supported when `ffmpeg` is available in PATH.

Pod-touch sampling source:

- Default is ingress logs (`POD_TOUCH_SOURCE=ingress`) to measure real load-balancer routing.
- Optional fallback: `POD_TOUCH_SOURCE=chat_service_logs`.

Output is written under `artifacts/load_test_<timestamp>/`:

- `load_summary.json`
- `pod_touch_samples.csv`
- `db_population_samples.csv`
- `pod_touch_chart.png`
- `db_population_chart.png`
- `pod_touch_evolution.gif|mp4` (optional)
- `db_population_evolution.gif|mp4` (optional)

## Compare multiple balancing strategies

Run one load-test pass per strategy and create comparison charts:

```bash
./scripts/run_lb_strategy_comparison.sh
```

Default strategies:

- `round_robin`
- `hash_client_id` (sticky by websocket query arg `client_id`)
- `ewma`

Example with custom duration/concurrency:

```bash
CLIENTS=30 DURATION_SECONDS=90 ./scripts/run_lb_strategy_comparison.sh
```

Comparison artifacts are written to `artifacts/lb_strategy_<timestamp>/` and include:

- `strategy_pod_touch_comparison.png`
- `strategy_db_growth_comparison.png`
