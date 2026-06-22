FROM ubuntu:22.04 AS build

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY backend /app/backend
RUN cmake -S /app/backend/services/message_queue -B /app/backend/services/message_queue/build \
    && cmake --build /app/backend/services/message_queue/build -j

FROM ubuntu:22.04
RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY --from=build /app/backend/services/message_queue/build/message_queue_worker /app/message_queue_worker

EXPOSE 9090
ENTRYPOINT ["/app/message_queue_worker"]
