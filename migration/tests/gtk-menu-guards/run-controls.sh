#!/bin/bash
# Source the pinned dependency environment and explicitly prepend repaired GTK.
set -euo pipefail
[ "$#" = 2 ] || { echo 'usage: run-controls.sh OUTPUT PINNED_REGISTRY'; exit 2; }
src=$(cd "$(dirname "$0")" && pwd)
mkdir -p "$1"; export GTK_GUARD_TEST_DIR=$(realpath "$1") GTK_GUARD_REGISTRY=$(realpath "$2")
read -r -a flags <<< "$(pkg-config --cflags --libs gtk+-3.0 atspi-2)"
cc -std=c11 -Wall -Wextra -Werror "$src/grab-controls.c" -o "$GTK_GUARD_TEST_DIR/grab-controls" "${flags[@]}"
cc -std=c11 -Wall -Wextra -Werror "$src/atspi-controls.c" -o "$GTK_GUARD_TEST_DIR/atspi-controls" "${flags[@]}"
cp "$src/dbus-loopback.conf" "$GTK_GUARD_TEST_DIR/dbus-loopback.conf"
G_DEBUG=fatal-warnings "$GTK_GUARD_TEST_DIR/grab-controls" > "$GTK_GUARD_TEST_DIR/grab-controls.log" 2>&1
bash "$src/run-atspi.sh"
