#!/bin/sh
# dcd container entrypoint: seeds the configuration, applies DCD_* environment
# overrides, bridges syslog to stdout and supervises dcd as PID 1's child.
set -eu

CONF_DIR="${DCD_CONFIG_DIR:-/etc/dcd}"
CONF_FILE="$CONF_DIR/dcd.conf"
DEFAULTS_DIR="${DCD_DEFAULTS_DIR:-/usr/share/dcd/conf-defaults}"
DCD_BIN="${DCD_BIN:-/usr/local/bin/dcd}"
SYSLOGD_BIN="${DCD_SYSLOGD_BIN:-/usr/local/bin/syslog-forward}"
SYSLOG_SOCKET="${DCD_SYSLOG_SOCKET:-/dev/log}"
HUB_USER="${DCD_RUN_USER:-daemon}"

# set_conf <key> <value>: replace key (even when commented out) or append
set_conf()
{
	if grep -qE "^[[:space:]]*#?[[:space:]]*$1[[:space:]]*=" "$CONF_FILE"; then
		sed -i -E "s|^[[:space:]]*#?[[:space:]]*$1[[:space:]]*=.*|$1 = $2|" "$CONF_FILE"
	else
		printf '%s = %s\n' "$1" "$2" >> "$CONF_FILE"
	fi
}

conf_val()
{
	sed -n -E "s/^[[:space:]]*$1[[:space:]]*=[[:space:]]*([^#[:space:]]+).*/\1/p" "$CONF_FILE" | tail -n1
}

if [ ! -f "$CONF_FILE" ]; then
	echo "dcd-entrypoint: seeding $CONF_DIR from $DEFAULTS_DIR"
	mkdir -p "$CONF_DIR"
	cp -a "$DEFAULTS_DIR"/. "$CONF_DIR"/
	# the image healthcheck uses the web endpoint: enable it on a fresh seed
	set_conf listen_port_http "${DCD_HTTP_PORT:-8080}"
else
	echo "dcd-entrypoint: using existing $CONF_FILE"
fi

# explicit environment overrides win on every start; leave them unset to
# manage the mounted configuration by hand
[ -n "${DCD_HUBNAME:-}" ]        && set_conf hubname             "$DCD_HUBNAME"
[ -n "${DCD_ADMIN_CONTACT:-}" ] && set_conf admin_contact        "$DCD_ADMIN_CONTACT"
[ -n "${DCD_BIND_ADDRESS:-}" ]  && set_conf bind_address         "$DCD_BIND_ADDRESS"
[ -n "${DCD_PORT:-}" ]          && set_conf listen_port_main     "$DCD_PORT"
[ -n "${DCD_CONS_PORT:-}" ]     && set_conf listen_port_cons     "$DCD_CONS_PORT"
[ -n "${DCD_HTTP_PORT:-}" ]     && set_conf listen_port_http     "$DCD_HTTP_PORT"
[ -n "${DCD_USER_LIMIT:-}" ]    && set_conf user_limit           "$DCD_USER_LIMIT"
[ -n "${DCD_MIN_SHARE:-}" ]     && set_conf min_share            "$DCD_MIN_SHARE"
[ -n "${DCD_MIN_SLOTS:-}" ]     && set_conf min_slots            "$DCD_MIN_SLOTS"
[ -n "${DCD_REGISTERED_ONLY:-}" ] && set_conf registered_only    "$DCD_REGISTERED_ONLY"
[ -n "${DCD_LOG_FACILITY:-}" ]  && set_conf log_facility         "$DCD_LOG_FACILITY"

# dcd setuids to the configured user and rewrites dcd.penalties in CONF_DIR
if [ "$(id -u)" = 0 ]; then
	chown -R "$HUB_USER" "$CONF_DIR"
else
	echo "dcd-entrypoint: not root, skipping chown of $CONF_DIR" >&2
fi

PORT="$(conf_val listen_port_main)"
CONS_PORT="$(conf_val listen_port_cons)"
HTTP_PORT="$(conf_val listen_port_http)"
echo "dcd-entrypoint: hub on $PORT/tcp+$PORT/udp, console ${CONS_PORT:-off}, web ${HTTP_PORT:-off}"

# dcd logs through syslog(3) only: bridge /dev/log to stdout
"$SYSLOGD_BIN" "$SYSLOG_SOCKET" &
SYSLOG_PID=$!

# dcd's openlog() is not retried after a failed connect: wait for the socket
i=0
while [ ! -S "$SYSLOG_SOCKET" ] && [ "$i" -lt 50 ]; do
	sleep 0.1
	i=$((i+1))
done
[ -S "$SYSLOG_SOCKET" ] || { echo "dcd-entrypoint: $SYSLOG_SOCKET never appeared" >&2; exit 1; }

# keep dcd in the foreground so signals and its exit code reach us
DCD_NO_DAEMON=1 "$DCD_BIN" &
HUB_PID=$!

TERM_SEEN=0
stop_hub()
{
	TERM_SEEN=1
	kill -TERM "$HUB_PID" 2>/dev/null || true
}
trap stop_hub TERM INT
trap 'kill -HUP "$HUB_PID" 2>/dev/null || true' HUP

echo "dcd-entrypoint: hub started (pid $HUB_PID)"

while kill -0 "$HUB_PID" 2>/dev/null; do
	sleep 1
done
HUB_RC=0
wait "$HUB_PID" || HUB_RC=$?

kill "$SYSLOG_PID" 2>/dev/null || true
wait "$SYSLOG_PID" 2>/dev/null || true

echo "dcd-entrypoint: hub exited (rc=$HUB_RC)"
if [ "$TERM_SEEN" = 1 ]; then
	exit 0
fi
exit "$HUB_RC"
