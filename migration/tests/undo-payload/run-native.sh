set -euo pipefail
source <native>/env.sh
exec 9>"$GIMP_PAINTER_BUILD_LOCK"; flock 9
R=<checkout>
S=<native>/source
B=<native>/build-port
E=<evidence>
cp "$R/app/tests/test-gimp-painter-surface.cpp" "$R/app/tests/test-painter-surface-undo-payload.inc" "$S/app/tests/"
meson compile -C "$B" -j4 gimp-painter-surface painter-tempbuf painter-mypaint-buffer-read >"$E/native-build.log" 2>&1
python3 "$S/tools/run-painter-xvfb.py" meson test -C "$B" --no-rebuild --num-processes 1 --print-errorlogs --logbase undo-payload-native gimp-painter-surface painter-tempbuf painter-mypaint-buffer-read >"$E/native.log" 2>&1
