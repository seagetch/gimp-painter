set -euo pipefail
source <native>/env.sh
exec 9>"$GIMP_PAINTER_BUILD_LOCK"; flock 9
R=<checkout>
S=<native>/source
B=<native>/build-port
E=<evidence>
cp "$R/app/tests/test-painter-projection-notify.inc" "$S/app/tests/"
meson compile -C "$B" -j4 painter-mypaint-tool >"$E/final-build.log" 2>&1
python3 "$S/tools/run-painter-xvfb.py" meson test -C "$B" --no-rebuild --num-processes 1 --print-errorlogs --logbase projection-final-regression painter-mypaint-tool gimp-painter-surface >"$E/final-regression.log" 2>&1
python3 "$E/run-mutations.py" --source "$S" --build "$B" --output "$E" >"$E/mutations.log" 2>&1
GIMP_TEST_PROJECTION_VISUAL_DIR="$E" python3 "$S/tools/run-painter-xvfb.py" meson test -C "$B" --no-rebuild --num-processes 1 --print-errorlogs --logbase projection-restored '--test-args=-p /painter-tool/projection/visible-update' painter-mypaint-tool >"$E/restored.log" 2>&1
cp "$R/migration/tests/build_projection_notify_sanitizers.py" "$S/migration/tests/"
python3 "$S/migration/tests/build_projection_notify_sanitizers.py" "$B" --report "$E/sanitizer-build.json" >"$E/sanitizer-build.log" 2>&1
python3 "$S/tools/run-painter-xvfb.py" python3 "$E/run-sanitizer.py" "$S" "$B" "$E" >"$E/sanitizer.log" 2>&1
