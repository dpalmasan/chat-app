FROM ubuntu:22.04 AS build

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    pkg-config \
    libmongoc-dev \
    libbson-dev \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY backend /app/backend
RUN cmake -S /app/backend/services/db_service -B /app/backend/services/db_service/build \
    && cmake --build /app/backend/services/db_service/build -j

FROM ubuntu:22.04
RUN apt-get update && apt-get install -y --no-install-recommends \
    libmongoc-1.0-0 \
    libbson-1.0-0 \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY --from=build /app/backend/services/db_service/build/db_service /app/db_service

EXPOSE 9091
ENTRYPOINT ["/app/db_service"]
