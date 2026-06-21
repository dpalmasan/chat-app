# Web Client (Flask)

Flask web UI plus a simple in-process load balancer/service discovery layer.

## Prerequisites

- `pyenv` installed
- Python `3.14.3` installed in pyenv

## Setup

```bash
cd clients/web_client
pyenv local 3.14.3
python -m venv .venv
source .venv/bin/activate
pip install -U pip
pip install -r requirements.txt
```

## Run

```bash
source .venv/bin/activate
python app.py
```

Then open http://127.0.0.1:5000

## Service Discovery Configuration

The Flask app acts as a simple load balancer front door and picks a WebSocket
chat-service instance for each logged-in user.

Configure discoverable chat-service instances with:

```bash
export CHAT_SERVICE_WS_INSTANCES="ws://127.0.0.1:8080/ws,ws://127.0.0.1:8081/ws"
```

If not set, it defaults to exactly the two endpoints above.

## Run E2E With Worker + C++ Services

Terminal 1 (db service):

```bash
cd backend/services/db_service
cmake -S . -B build
cmake --build build
./build/db_service
```

Terminal 2 (message queue worker):

```bash
cd backend/services/message_queue
cmake -S . -B build
cmake --build build
./build/message_queue_worker
```

Terminal 3 (chat service instance A):

```bash
cd backend/services/chat_service
cmake -S . -B build
cmake --build build
./build/chat_service
```

Terminal 4 (chat service instance B on another port):

```bash
cd backend/services/chat_service
./build/chat_service 8081
```

Terminal 5 (Flask UI):

```bash
cd clients/web_client
source .venv/bin/activate
python app.py
```

Testing flow:

1. Open two browser tabs on http://127.0.0.1:5000
2. In tab A login as `user_a`
3. In tab B login as `user_b`
4. The load balancer/service discovery selects WebSocket instances automatically
5. Send message from `user_a` to `user_b` and verify cross-instance delivery

## Notes

- Flask serves the UI and lightweight HTTP APIs (`/api/login`, `/api/users`, `/api/history`, `/api/logout`).
- Flask currently implements a simple in-memory service discovery/load-balancing strategy (sticky user assignment + least-loaded selection for new users).
- chat_service publishes writes to message_queue worker.
- db_service owns message persistence/history (`message_id`, `created_at`, pending history).
