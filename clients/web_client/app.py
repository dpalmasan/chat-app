from __future__ import annotations

import json
from datetime import UTC, datetime

from flask import Flask, render_template
from flask_sock import Sock

app = Flask(__name__)
sock = Sock(app)


@app.get("/")
def index() -> str:
    return render_template("index.html")


@sock.route("/ws")
def chat_socket(ws) -> None:
    """Temporary websocket endpoint for local web-client testing."""
    while True:
        raw_message = ws.receive()
        if raw_message is None:
            break

        try:
            payload = json.loads(raw_message)
        except json.JSONDecodeError:
            ws.send(
                json.dumps(
                    {
                        "type": "error",
                        "message": "Invalid JSON payload",
                    }
                )
            )
            continue

        payload["created_at"] = datetime.now(UTC).isoformat()
        payload["type"] = "chat_message"
        ws.send(json.dumps(payload))


if __name__ == "__main__":
    app.run(debug=True)
