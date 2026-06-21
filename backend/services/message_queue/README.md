# Message Queue Worker

Dedicated worker service that owns:

- In-memory message queue (blocking)
- Subscriber that consumes queue events and forwards writes to db_service

The worker exposes a simple TCP protocol on port 9090 used by chat service instances.

## Build

```bash
cd backend/services/message_queue
cmake -S . -B build
cmake --build build
```

## Run

```bash
./build/message_queue_worker
```

Environment:

- `DB_SERVICE_HOST` (default: `127.0.0.1`)
- `DB_SERVICE_PORT` (default: `9091`)

## Protocol (line based)

- `PUBLISH|from|to|content`

Fields are URL-encoded to preserve message content safely.
