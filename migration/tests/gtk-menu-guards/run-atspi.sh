#!/bin/bash
set -euo pipefail
r="${GTK_GUARD_TEST_DIR:?set private test output directory}"
export NO_AT_BRIDGE=0 G_DEBUG=fatal-warnings ATSPI_DISABLE_P2P=1
# Private nonce-protected loopback bus; no socket outside loopback, no anonymous auth.
dbus-daemon --nofork --config-file="$r/dbus-loopback.conf" --print-address > "$r/bus-address" 2> "$r/dbus-server.log" & bus=$!
for i in $(seq 1 50); do test -s "$r/bus-address" && break; sleep .1; done
export DBUS_SESSION_BUS_ADDRESS="$(head -1 "$r/bus-address")"
export AT_SPI_BUS_ADDRESS="$DBUS_SESSION_BUS_ADDRESS"
"${GTK_GUARD_REGISTRY:?set pinned at-spi2-registryd path}" > "$r/registry.log" 2>&1 & registry=$!
"$r/atspi-controls" > "$r/atspi-server.log" 2>&1 & p=$!
trap 'kill "$p" "$registry" "$bus" 2>/dev/null || true' EXIT
"$r/atspi-controls" client > "$r/atspi-client.log" 2>&1
wait "$p"
