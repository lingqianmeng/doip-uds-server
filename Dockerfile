FROM debian:bookworm-slim AS build

RUN apt-get update \
    && apt-get install --no-install-recommends -y \
        ca-certificates \
        cmake \
        g++ \
        ninja-build \
        python3 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

RUN cmake -S . -B /build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DDOIP_BUILD_TESTS=ON

RUN cmake --build /build --parallel 2

RUN ctest --test-dir /build --output-on-failure

FROM debian:bookworm-slim AS runtime

RUN apt-get update \
    && apt-get install --no-install-recommends -y libstdc++6 \
    && rm -rf /var/lib/apt/lists/*

COPY --from=build /build/doip_server /usr/local/bin/doip_server
COPY config/ecu_config.json /etc/doip-uds-server/ecu_config.json

USER 65532:65532
EXPOSE 13400/tcp 13400/udp
STOPSIGNAL SIGTERM
ENTRYPOINT ["/usr/local/bin/doip_server"]
CMD ["--config", "/etc/doip-uds-server/ecu_config.json"]
