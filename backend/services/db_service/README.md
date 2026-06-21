# DB Service

Dedicated persistence/history service for chat messages.

Current implementation:

- In-memory message store
- TCP API for message writes and history polling

## Build

```bash
cd backend/services/db_service
cmake -S . -B build
cmake --build build
```

## Run

```bash
./build/db_service
```

## Protocol (line based)

- `SAVE|from|to|content`
- `LOAD_PENDING|recipient`
- `MARK_DELIVERED|recipient|message_id`

Fields are URL-encoded to preserve message content safely.
