#!/usr/bin/env bash
set -euo pipefail

# Reset chat application data in MongoDB and seed default users.
# Optional environment variables:
#   MONGODB_URI      (default: mongodb://127.0.0.1:27017)
#   MONGODB_DB_NAME  (default: chat_app)

MONGODB_URI="${MONGODB_URI:-mongodb://127.0.0.1:27017}"
MONGODB_DB_NAME="${MONGODB_DB_NAME:-chat_app}"

if ! command -v mongosh >/dev/null 2>&1; then
  echo "Error: mongosh not found in PATH" >&2
  exit 1
fi

echo "Resetting database '$MONGODB_DB_NAME' at '$MONGODB_URI'..."

mongosh --quiet "$MONGODB_URI/$MONGODB_DB_NAME" <<'MONGO_SCRIPT'
const now = Date.now();

const userDocs = [
  { user_id: "user_a", is_online: false, active_connections: 0, last_active_at_ms: now },
  { user_id: "user_b", is_online: false, active_connections: 0, last_active_at_ms: now },
  { user_id: "user_c", is_online: false, active_connections: 0, last_active_at_ms: now },
  { user_id: "user_d", is_online: false, active_connections: 0, last_active_at_ms: now },
  { user_id: "user_e", is_online: false, active_connections: 0, last_active_at_ms: now },
];

// Keep collections in place, but wipe chat data and re-seed users.
db.messages.deleteMany({});
db.users.deleteMany({});
db.users.insertMany(userDocs);

print("messages count:", db.messages.countDocuments());
print("users count:", db.users.countDocuments());
print("Seeded users:", db.users.find({}, { _id: 0, user_id: 1 }).sort({ user_id: 1 }).toArray());
MONGO_SCRIPT

echo "Done."
