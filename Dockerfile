FROM ubuntu:24.04 AS builder

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        build-essential \
        cmake && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY . .

RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && \
    cmake --build build --config Release

FROM ubuntu:24.04

WORKDIR /app

COPY --from=builder /app/build/http_server /app/http_server
COPY --from=builder /app/public /app/public

RUN mkdir -p /app/uploads

ENV PORT=10000

EXPOSE 10000

CMD ["./http_server"]