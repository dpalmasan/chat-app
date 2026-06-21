# Web Client (Flask)

Simple Step 1 web client UI for the chat app specification.

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
2. In tab A set endpoint to `ws://127.0.0.1:8080/ws`, set `client_id=user_a`, click `Connect`
3. In tab B set endpoint to `ws://127.0.0.1:8081/ws`, set `client_id=user_b`, click `Connect`
4. In tab A send to `message_to=user_b`
5. Verify tab B receives the message JSON even though users are on different chat service instances

## Notes

- Flask serves only the UI page.
- The browser connects directly to the C++ WebSocket endpoint (`ws://127.0.0.1:8080/ws?client_id=<id>`).
- chat_service publishes writes to message_queue worker.
- db_service owns message persistence/history (`message_id`, `created_at`, pending history).
