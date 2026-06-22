#!/usr/bin/env python3
"""Generate websocket chat traffic against the ingress endpoint.

This script opens many websocket connections and periodically sends messages
between seeded users (user_a..user_e) to stress chat-service and persistence.
"""

from __future__ import annotations

import argparse
import asyncio
import json
import time
from dataclasses import dataclass
from urllib.parse import quote

try:
    import websockets
except ImportError as exc:  # pragma: no cover - runtime dependency guard
    raise SystemExit(
        "Missing dependency: websockets. Install with: pip install websockets"
    ) from exc


DEFAULT_USERS = ["user_a", "user_b", "user_c", "user_d", "user_e"]


@dataclass
class WorkerStats:
    sent: int = 0
    recv: int = 0
    errors: int = 0


async def worker(
    worker_id: int,
    endpoint: str,
    users: list[str],
    send_interval_s: float,
    duration_s: int,
) -> WorkerStats:
    stats = WorkerStats()
    sender = users[worker_id % len(users)]
    recipient = users[(worker_id + 1) % len(users)]
    stop_at = time.monotonic() + duration_s

    ws_url = endpoint
    sep = "&" if "?" in ws_url else "?"
    ws_url = f"{ws_url}{sep}client_id={quote(sender, safe='')}"

    while time.monotonic() < stop_at:
        try:
            async with websockets.connect(
                ws_url,
                open_timeout=5,
                ping_interval=20,
                ping_timeout=20,
                close_timeout=2,
            ) as socket:
                while time.monotonic() < stop_at:
                    payload = {
                        "message_from": sender,
                        "message_to": recipient,
                        "content": f"load-test from {sender} via worker {worker_id}",
                    }
                    await socket.send(json.dumps(payload))
                    stats.sent += 1

                    try:
                        await asyncio.wait_for(socket.recv(), timeout=0.2)
                        stats.recv += 1
                    except asyncio.TimeoutError:
                        pass

                    await asyncio.sleep(send_interval_s)
        except Exception:
            stats.errors += 1
            await asyncio.sleep(0.2)

    return stats


async def run(args: argparse.Namespace) -> int:
    users = [u.strip() for u in args.users.split(",") if u.strip()]
    if not users:
        raise SystemExit("--users must provide at least one user id")

    tasks = [
        asyncio.create_task(
            worker(
                worker_id=i,
                endpoint=args.endpoint,
                users=users,
                send_interval_s=args.send_interval,
                duration_s=args.duration,
            )
        )
        for i in range(args.clients)
    ]

    started = time.time()
    results = await asyncio.gather(*tasks)
    elapsed = time.time() - started

    total_sent = sum(s.sent for s in results)
    total_recv = sum(s.recv for s in results)
    total_errors = sum(s.errors for s in results)

    summary = {
        "clients": args.clients,
        "duration_s": args.duration,
        "elapsed_s": round(elapsed, 2),
        "endpoint": args.endpoint,
        "users": users,
        "total_sent": total_sent,
        "total_recv": total_recv,
        "total_errors": total_errors,
        "send_rate_msgs_per_sec": round(total_sent / max(elapsed, 0.001), 2),
    }

    print(json.dumps(summary, indent=2))
    return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Websocket load generator for chat.local")
    parser.add_argument("--endpoint", default="ws://chat.local/ws", help="Websocket endpoint base URL")
    parser.add_argument("--clients", type=int, default=25, help="Concurrent websocket clients")
    parser.add_argument("--duration", type=int, default=90, help="Test duration in seconds")
    parser.add_argument(
        "--send-interval",
        type=float,
        default=0.7,
        help="Seconds between sends per connected client",
    )
    parser.add_argument(
        "--users",
        default=",".join(DEFAULT_USERS),
        help="Comma-separated seeded user ids used for client_id/message_from",
    )
    return parser


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()
    return asyncio.run(run(args))


if __name__ == "__main__":
    raise SystemExit(main())
