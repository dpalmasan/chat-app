from __future__ import annotations

import json
from itertools import count
from threading import Lock
from datetime import UTC, datetime

from flask import Flask, render_template
from flask_sock import Sock

app = Flask(__name__)
sock = Sock(app)
message_id_counter_ = count(start=1)
message_id_lock_ = Lock()


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

        # Backend owns primary key generation; ignore any client-supplied id.
        payload.pop("message_id", None)
        with message_id_lock_:
            payload["message_id"] = next(message_id_counter_)
        payload["created_at"] = datetime.now(UTC).isoformat()
        payload["type"] = "chat_message"
        ws.send(json.dumps(payload))


if __name__ == "__main__":
    app.run(debug=True)
