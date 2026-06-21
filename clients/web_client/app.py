from __future__ import annotations

"""Flask web client backend for login, presence, and history APIs.

This module serves the web UI and provides thin HTTP endpoints that proxy to
the db_service TCP protocol used by backend services.
"""

import os
import socket
from datetime import datetime, timezone
from urllib.parse import quote, unquote

from flask import Flask, jsonify, render_template, request

app = Flask(__name__)


DB_SERVICE_HOST = os.getenv("DB_SERVICE_HOST", "127.0.0.1")
DB_SERVICE_PORT = int(os.getenv("DB_SERVICE_PORT", "9091"))


def _read_line(sock: socket.socket) -> str:
    """Read one newline-delimited UTF-8 response line from a connected socket."""
    chunks: list[bytes] = []
    while True:
        chunk = sock.recv(1)
        if not chunk:
            raise RuntimeError("connection closed while reading db service response")
        if chunk == b"\n":
            break
        chunks.append(chunk)
    return b"".join(chunks).decode("utf-8")


def _db_request(command: str) -> str:
    """Send one command to db_service and return the payload of an OK response.

    Raises:
        RuntimeError: If db_service returns an error response or malformed data.
    """
    with socket.create_connection((DB_SERVICE_HOST, DB_SERVICE_PORT), timeout=3) as sock:
        sock.sendall(f"{command}\n".encode("utf-8"))
        line = _read_line(sock)

    if line.startswith("OK|"):
        return line[3:]
    if line.startswith("ERR|"):
        raise RuntimeError(line[4:])
    raise RuntimeError("invalid response from db service")


def _parse_epoch_ms(epoch_ms_text: str) -> str:
    """Convert epoch milliseconds into an ISO-8601 UTC timestamp string."""
    timestamp = int(epoch_ms_text) / 1000.0
    dt = datetime.fromtimestamp(timestamp, tz=timezone.utc)
    return dt.isoformat().replace("+00:00", "Z")


def _parse_user_entry(entry: str) -> dict[str, str | bool]:
    """Parse one serialized user-presence entry from db_service."""
    tokens = entry.split("|")
    if len(tokens) != 3:
        raise RuntimeError("invalid user payload")

    return {
        "user_id": unquote(tokens[0]),
        "is_online": tokens[1] == "online",
        "status": tokens[1],
        "last_active_at": _parse_epoch_ms(tokens[2]),
    }


def _parse_message_entry(entry: str) -> dict[str, str | int]:
    """Parse one serialized chat message entry from db_service."""
    tokens = entry.split("|")
    if len(tokens) != 5:
        raise RuntimeError("invalid message payload")

    return {
        "message_id": int(tokens[0]),
        "message_from": unquote(tokens[1]),
        "message_to": unquote(tokens[2]),
        "content": unquote(tokens[3]),
        "created_at": _parse_epoch_ms(tokens[4]),
    }


@app.get("/")
def index() -> str:
    """Render the web client single-page interface."""
    return render_template("index.html")


@app.post("/api/login")
def login() -> tuple[dict[str, object], int]:
    """Login a user through db_service and return current presence metadata."""
    body = request.get_json(silent=True) or {}
    user_id = str(body.get("user_id", "")).strip()
    if not user_id:
        return {"ok": False, "error": "user_id is required"}, 400

    try:
        payload = _db_request(f"LOGIN|{quote(user_id, safe='')}")
        user = _parse_user_entry(payload)
        return {"ok": True, "user": user}, 200
    except Exception as ex:  # noqa: BLE001 - tiny prototype endpoint
        return {"ok": False, "error": str(ex)}, 400


@app.get("/api/users")
def list_users() -> tuple[dict[str, object], int]:
    """Return all users with presence state from db_service."""
    try:
        payload = _db_request("LIST_USERS")
        users = []
        for entry in payload.split(";"):
            if not entry:
                continue
            users.append(_parse_user_entry(entry))
        return jsonify({"ok": True, "users": users}), 200
    except Exception as ex:  # noqa: BLE001 - tiny prototype endpoint
        return {"ok": False, "error": str(ex)}, 500


@app.get("/api/history")
def history() -> tuple[dict[str, object], int]:
    """Return full chat history for a user_id from db_service."""
    user_id = request.args.get("user_id", "").strip()
    if not user_id:
        return {"ok": False, "error": "user_id is required"}, 400

    try:
        payload = _db_request(f"LOAD_HISTORY|{quote(user_id, safe='')}")
        messages = []
        for entry in payload.split(";"):
            if not entry:
                continue
            messages.append(_parse_message_entry(entry))
        return jsonify({"ok": True, "messages": messages}), 200
    except Exception as ex:  # noqa: BLE001 - tiny prototype endpoint
        return {"ok": False, "error": str(ex)}, 500


if __name__ == "__main__":
    app.run(debug=True)
