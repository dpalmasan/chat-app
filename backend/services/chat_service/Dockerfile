FROM ubuntu:22.04 AS build

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    pkg-config \
    libssl-dev \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY backend /app/backend
RUN cmake -S /app/backend/services/chat_service -B /app/backend/services/chat_service/build \
    && cmake --build /app/backend/services/chat_service/build -j

FROM ubuntu:22.04
RUN apt-get update && apt-get install -y --no-install-recommends \
    libssl3 \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY --from=build /app/backend/services/chat_service/build/chat_service /app/chat_service

EXPOSE 8080
ENTRYPOINT ["/app/chat_service"]
