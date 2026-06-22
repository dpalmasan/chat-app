from __future__ import annotations

"""Flask web client backend for login, presence, and history APIs.

This module serves the web UI and provides thin HTTP endpoints that proxy to
the db_service TCP protocol used by backend services.
"""

import os
import socket
import threading
from datetime import datetime, timezone
from urllib.parse import quote, unquote

from flask import Flask, jsonify, render_template, request

app = Flask(__name__)


DB_SERVICE_HOST = os.getenv("DB_SERVICE_HOST", "127.0.0.1")
DB_SERVICE_PORT = int(os.getenv("DB_SERVICE_PORT", "9091"))
SERVICE_DISCOVERY_MODE = os.getenv("CHAT_SERVICE_DISCOVERY_MODE", "ingress").strip().lower()
CHAT_WS_PATH = os.getenv("CHAT_WS_PATH", "/ws")
CHAT_SERVICE_WS_ENDPOINT = os.getenv("CHAT_SERVICE_WS_ENDPOINT", "").strip()


class ServiceDiscoveryLoadBalancer:
    """Simple in-memory service discovery + load balancer (dev-only).

    This is only intended for local development when running multiple
    chat_service instances outside Kubernetes/Ingress. In production,
    use ingress-based routing instead.
    """

    def __init__(self, websocket_instances: list[str]) -> None:
        if not websocket_instances:
            raise ValueError("at least one websocket instance is required")
        self._instances = sorted(websocket_instances)
        self._lock = threading.Lock()
        self._user_to_instance: dict[str, str] = {}
        self._instance_load: dict[str, int] = {instance: 0 for instance in self._instances}

    def assign(self, user_id: str) -> str:
        """Return a sticky websocket instance for the given user."""
        with self._lock:
            if user_id in self._user_to_instance:
                return self._user_to_instance[user_id]

            selected = min(
                self._instances,
                key=lambda instance: (self._instance_load[instance], instance),
            )
            self._user_to_instance[user_id] = selected
            self._instance_load[selected] += 1
            return selected

    def release(self, user_id: str) -> None:
        """Release assignment for a user, decreasing the selected instance load."""
        with self._lock:
            assigned = self._user_to_instance.pop(user_id, None)
            if not assigned:
                return
            current_load = self._instance_load.get(assigned, 0)
            self._instance_load[assigned] = max(0, current_load - 1)


def _load_websocket_instances() -> list[str]:
    """Load discoverable websocket instances for dev-only load balancing."""
    configured = os.getenv("CHAT_SERVICE_WS_INSTANCES", "")
    if configured.strip():
        return [entry.strip() for entry in configured.split(",") if entry.strip()]
    return [
        "ws://127.0.0.1:8080/ws",
        "ws://127.0.0.1:8081/ws",
    ]


service_discovery = ServiceDiscoveryLoadBalancer(_load_websocket_instances())


def _normalize_ws_path(path: str) -> str:
    normalized = path.strip() or "/ws"
    if not normalized.startswith("/"):
        normalized = f"/{normalized}"
    return normalized


def _resolve_ingress_ws_endpoint() -> str:
    """Resolve websocket endpoint through ingress/current host.

    Priority:
    1) Explicit CHAT_SERVICE_WS_ENDPOINT override.
    2) Request host + ws/wss + CHAT_WS_PATH.
    """
    if CHAT_SERVICE_WS_ENDPOINT:
        return CHAT_SERVICE_WS_ENDPOINT

    forwarded_proto = request.headers.get("X-Forwarded-Proto", "")
    scheme = "wss" if forwarded_proto == "https" or request.scheme == "https" else "ws"
    host = request.headers.get("X-Forwarded-Host") or request.host
    return f"{scheme}://{host}{_normalize_ws_path(CHAT_WS_PATH)}"


def _resolve_websocket_endpoint(user_id: str) -> tuple[str, str]:
    """Return websocket endpoint and discovery mode label."""
    if SERVICE_DISCOVERY_MODE == "dev_lb":
        return service_discovery.assign(user_id), "dev_lb"
    return _resolve_ingress_ws_endpoint(), "ingress"


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
        websocket_endpoint, discovery_mode = _resolve_websocket_endpoint(user_id)
        return {
            "ok": True,
            "user": user,
            "service_discovery": {
                "websocket_endpoint": websocket_endpoint,
                "mode": discovery_mode,
            },
        }, 200
    except Exception as ex:  # noqa: BLE001 - tiny prototype endpoint
        return {"ok": False, "error": str(ex)}, 400


@app.post("/api/logout")
def logout() -> tuple[dict[str, object], int]:
    """Release dev load-balancer assignment for a user when logging out."""
    body = request.get_json(silent=True) or {}
    user_id = str(body.get("user_id", "")).strip()
    if not user_id:
        return {"ok": False, "error": "user_id is required"}, 400

    if SERVICE_DISCOVERY_MODE == "dev_lb":
        service_discovery.release(user_id)
    return {"ok": True}, 200


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
    app.run(
        host=os.getenv("FLASK_RUN_HOST", "0.0.0.0"),
        port=int(os.getenv("PORT", "5000")),
        debug=os.getenv("FLASK_DEBUG", "0") == "1",
    )
