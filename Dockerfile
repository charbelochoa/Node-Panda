FROM ubuntu:24.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    ninja-build \
    git \
    ca-certificates \
    pkg-config \
    libgl1-mesa-dev \
    libx11-dev \
    libxrandr-dev \
    libxinerama-dev \
    libxcursor-dev \
    libxi-dev \
    libwayland-dev \
    libxkbcommon-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

RUN cmake -S . -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
    && cmake --build build --config Release

FROM ubuntu:24.04 AS runtime

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    libgl1 \
    libx11-6 \
    libxrandr2 \
    libxinerama1 \
    libxcursor1 \
    libxi6 \
    ca-certificates \
    postgresql-client \
    default-mysql-client \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY --from=builder /src/build/NodePanda /app/NodePanda
COPY --from=builder /src/data /app/data

ENV NODEPANDA_HOST=0.0.0.0
ENV NODEPANDA_PORT=8042
ENV NODEPANDA_RTC_PORT=8043
ENV NODEPANDA_DB_ENGINE=postgresql
ENV NODEPANDA_DB_MODE=server
ENV NODEPANDA_DB_HOST=postgres
ENV NODEPANDA_DB_PORT=5432
ENV NODEPANDA_DB_NAME=nodepanda
ENV NODEPANDA_DB_USER=nodepanda
ENV NODEPANDA_DB_PASSWORD=nodepanda_secret

EXPOSE 8042 8043
VOLUME ["/app/data"]

ENTRYPOINT ["/app/NodePanda"]
CMD ["--server", "--port", "8042", "--no-auth-prompt"]
