#!/usr/bin/env bash
# Source the chosen GTK dependency environment before this script.
set -euo pipefail
if [[ $# != 2 || ( $2 != --expect-known-defect && $2 != --check-fixed ) ]]; then
  echo "Usage: $0 OUTPUT_DIRECTORY --expect-known-defect|--check-fixed" >&2
  exit 2
fi
source_dir="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$1"
output="$(cd "$1" && pwd)"
mode="$2"
read -r -a gtk_flags <<< "$(pkg-config --cflags --libs gtk+-3.0)"
cc -std=c11 -g -O0 -Wall -Wextra -Werror \
  "$source_dir/gtk-menu-repro.c" -o "$output/gtk-menu-repro" \
  "${gtk_flags[@]}" > "$output/build.log" 2>&1
sha256sum "$source_dir/gtk-menu-repro.c" "$output/gtk-menu-repro" \
  > "$output/identity.txt"
ulimit -c 0
set +e
"$output/gtk-menu-repro" "$mode" > "$output/stdout.log" 2> "$output/stderr.log"
result=$?
set -e
printf '%s\n' "$result" > "$output/exit-code.txt"
cat "$output/stdout.log"
printf 'GTK_MENU_REPRO_EXIT=%s\n' "$result"
exit "$result"
