#!/bin/bash
set -euo pipefail
source /workspace/shared/gimp-native-baseline/env.sh
exec 9>"$GIMP_PAINTER_BUILD_LOCK"; flock 9
src=/workspace/shared/gimp-native-baseline/source
build=/workspace/shared/gimp-native-baseline/build-port
out=/workspace/shared/filter-signal-fresh
ninja -C "$build" app/tests/gimp-filter-layer > "$out/build.log" 2>&1
for case in dependency_walk_does_not_resolve_pending_names changed_dependency_invalidation_can_close_owner dependency_connections_close_during_reorder dependency_connections_preserve_new_stack; do
 python3 "$src/tools/run-painter-xvfb.py" meson test -C "$build" --no-rebuild --num-processes 1 --print-errorlogs --logbase="filter-signal-$case" gimp-filter-layer --test-args="-p /gimp-filter-layer/$case" > "$out/$case.log" 2>&1
 cp "$build/meson-logs/filter-signal-$case.json" "$out/$case.json"
done
flock -u 9
printf "normal stage completed; lock released\n" > "$out/normal-stage-complete"
