# DB Service

Dedicated persistence/history service for chat messages.

Current implementation:

- MongoDB-backed message store (default)
- In-memory store fallback (`DB_BACKEND=in_memory`)
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

Default runtime uses MongoDB at `mongodb://127.0.0.1:27017` with database `chat_app`.

Optional environment variables:

```bash
export MONGODB_URI="mongodb://127.0.0.1:27017"
export MONGODB_DB_NAME="chat_app"
./build/db_service
```

In-memory fallback:

```bash
DB_BACKEND=in_memory ./build/db_service
```

## MongoDB Setup (Ubuntu/Pop!_OS)

If needed, install and start MongoDB:

```bash
sudo apt-get install -y mongodb-org
sudo systemctl start mongod
sudo systemctl is-active mongod
```

## Protocol (line based)

- `SAVE|from|to|content`
- `LOAD_PENDING|recipient`
- `LOAD_HISTORY|user_id`
- `MARK_DELIVERED|recipient|message_id`
- `LOGIN|user_id`
- `USER_EXISTS|user_id`
- `SET_PRESENCE|user_id|online|offline`
- `LIST_USERS`

Fields are URL-encoded to preserve message content safely.
