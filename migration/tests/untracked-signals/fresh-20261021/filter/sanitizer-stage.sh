#!/bin/bash
set -euo pipefail
source /workspace/shared/gimp-native-baseline/env.sh
exec 9>"$GIMP_PAINTER_BUILD_LOCK"; flock 9
src=/workspace/shared/gimp-native-baseline/source
build=/workspace/shared/gimp-native-baseline/build-port
out=/workspace/shared/filter-signal-fresh
python3 "$src/tools/run-painter-xvfb.py" python3 "$src/migration/tests/run_filter_layer_sanitizers.py" "$build" --report "$out/sanitizers.json" --test-path /gimp-filter-layer/dependency_walk_does_not_resolve_pending_names --test-path /gimp-filter-layer/changed_dependency_invalidation_can_close_owner --test-path /gimp-filter-layer/dependency_connections_close_during_reorder --test-path /gimp-filter-layer/dependency_connections_preserve_new_stack > "$out/sanitizer-build-run.log" 2>&1
