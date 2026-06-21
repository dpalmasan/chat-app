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

## Run E2E With C++ Service

Terminal 1 (C++ chat service):

```bash
cd backend/services/chat_service
cmake -S . -B build
cmake --build build
./build/chat_service
```

Terminal 2 (Flask UI):

```bash
cd clients/web_client
source .venv/bin/activate
python app.py
```

Testing flow:

1. Open two browser tabs on http://127.0.0.1:5000
2. In tab A set `client_id=user_a`, click `Connect`
3. In tab B set `client_id=user_b`, click `Connect`
4. In tab A send to `message_to=user_b`
5. Verify both tabs receive the message JSON and the C++ service logs persistence

## Notes

- Flask serves only the UI page.
- The browser connects directly to the C++ WebSocket endpoint (`ws://127.0.0.1:8080/ws?client_id=<id>`).
- `message_id` and `created_at` are owned by the C++ backend.
