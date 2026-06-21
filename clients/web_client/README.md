# Web Client (Flask)

Simple Step 1 web client boilerplate for the chat app specification.

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

## Notes

- Uses Flask for the web app and `flask-sock` for a temporary WebSocket endpoint.
- Current WebSocket endpoint (`/ws`) echoes incoming chat payloads and stamps `created_at` server-side.
- This is a client boilerplate; integration with the C++ chat service can be added next.
