# --- Stage 1 : compilation (jetée ensuite) ---
FROM alpine:3.20 AS builder

RUN apk add --no-cache build-base

WORKDIR /src
COPY Makefile ./
COPY common ./common
COPY fs ./fs
COPY ipc ./ipc
COPY kernel ./kernel
COPY shell ./shell

# Binaire statique, optimisé, sans symboles de debug
RUN make clean && \
    make all \
      CFLAGS="-Wall -Wextra -std=c11 -O2 -I." \
      LDFLAGS="-static" && \
    strip bin/shell && \
    mkdir -p /out/app/data && \
    cp bin/shell /out/app/shell

# --- Stage 2 : image finale minimale ---
FROM scratch

WORKDIR /app
COPY --from=builder /out/app /app

VOLUME ["/app/data"]

# Conteneur = juste le shell ; data/ est monté ou créé au premier lancement
ENTRYPOINT ["/app/shell"]
