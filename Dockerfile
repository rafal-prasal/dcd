# syntax=docker/dockerfile:1
#
# Multi-stage build of dcd (Direct Connect hub daemon).
#
#   docker build -t dcd:1.1.1 .
#   docker run -d -p 411:411 -p 411:411/udp -p 511:511 -p 8080:8080 \
#       -e DCD_HUBNAME="My Hub" -v dcd-conf:/etc/dcd dcd:1.1.1

##################### build stage #####################
FROM debian:bookworm AS build

RUN apt-get update \
 && apt-get install -y --no-install-recommends \
      build-essential cmake libssl-dev \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

# unit tests run as part of the image build; the long perf benches are excluded
RUN cmake -B build -S . \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX=/usr \
      -DDCD_CONFIG_DIR=/etc/dcd \
      -DDCD_BUILD_TESTS=ON \
 && cmake --build build --parallel "$(nproc)" \
 && ctest --test-dir build --output-on-failure -LE perf \
 && cmake --install build

# small helper that bridges dcd's syslog-only logging to container stdout
RUN gcc -O2 -D_GNU_SOURCE -Wall -Wextra -o /usr/local/bin/syslog-forward \
      docker/syslog-forward.c

#################### runtime stage ####################
FROM debian:bookworm-slim AS runtime

RUN apt-get update \
 && apt-get install -y --no-install-recommends libssl3 \
 && rm -rf /var/lib/apt/lists/*

# dcd binary (CONFIGDIR=/etc/dcd, drop-privileges user 'daemon' - present
# in the Debian base image) plus the adduser helper used as:
#   docker exec -it <container> dcd.adduser
COPY --from=build /usr/sbin/dcd /usr/local/bin/dcd
COPY --from=build /usr/sbin/dcd.adduser /usr/local/bin/dcd.adduser
COPY --from=build /usr/local/bin/syslog-forward /usr/local/bin/syslog-forward

# pristine config used to seed a fresh /etc/dcd volume; edit the mounted
# copy (or the DCD_* environment overrides) to configure the hub
COPY --from=build /etc/dcd /usr/share/dcd/conf-defaults

COPY docker/docker-entrypoint.sh /usr/local/bin/docker-entrypoint.sh
RUN chmod +x /usr/local/bin/docker-entrypoint.sh

# hub config + penalties state
VOLUME /etc/dcd

# 411/tcp+udp: NMDC clients, 511/tcp: remote console, 8080/tcp: web console + /healthcheck
EXPOSE 411/tcp 411/udp 511/tcp 8080/tcp

# checks the web endpoint /healthcheck (unhealthy when the user-table lock
# is held); skipped (healthy) when listen_port_http is 0 in the config
HEALTHCHECK --interval=30s --timeout=5s --start-period=15s --retries=3 \
  CMD bash -c 'p=$(sed -n -E "s/^[[:space:]]*listen_port_http[[:space:]]*=[[:space:]]*([0-9]+).*/\1/p" /etc/dcd/dcd.conf | tail -n1); [ "${p:-0}" -eq 0 ] && exit 0; exec 3<>/dev/tcp/127.0.0.1/"$p" && printf "GET /healthcheck HTTP/1.0\r\n\r\n" >&3 && head -n1 <&3 | grep -q " 200 "'

ENTRYPOINT ["/usr/local/bin/docker-entrypoint.sh"]
