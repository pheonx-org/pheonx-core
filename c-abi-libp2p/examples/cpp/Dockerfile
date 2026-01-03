#
# Build linux/amd64 Rust C-ABI library, then build the C++ client that dlopen()s it.
#
# Recommended invocation (from repo root):
#
#   docker buildx build --platform linux/amd64 \
#     -f c-abi-libp2p/examples/cpp/Dockerfile \
#     --target artifacts \
#     --output type=local,dest=./c-abi-libp2p/examples/cpp/dist \
#     .
#

FROM --platform=linux/amd64 rust:1.91 AS rust_builder

WORKDIR /src
COPY ../../../ ./c-abi-libp2p

WORKDIR /src/c-abi-libp2p
# Explicit target so the output path is stable under target/<triple>/...
RUN ls -lah /src/c-abi-libp2p &&cargo build --release --target x86_64-unknown-linux-gnu


FROM --platform=linux/amd64 debian:bookworm AS cpp_builder

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
  && rm -rf /var/lib/apt/lists/*

WORKDIR /build
COPY ./ ./cpp

WORKDIR /build/cpp
RUN cmake -S . -B build-docker -DCMAKE_BUILD_TYPE=Release
RUN cmake --build build-docker --config Release -j"$(nproc)"


FROM --platform=linux/amd64 gcr.io/distroless/cc-debian12 AS runtime

WORKDIR /app

COPY --from=rust_builder /src/c-abi-libp2p/target/release/libcabi_rust_libp2p.so /app/
COPY --from=cpp_builder /build/cpp/build-docker/ping /app/

CMD ["/app/ping", "--use-quic", "--lport", "41001", "--dport", "41002"]