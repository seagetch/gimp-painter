set -euo pipefail
source <native>/env.sh
exec 9>"$GIMP_PAINTER_BUILD_LOCK"; flock 9
R=<checkout>
S=<native>/source
B=<native>/build-port
E=<evidence>
cp "$R/app/tests/test-painter-mypaint-precision.cpp" "$R/app/tests/test-painter-precision-refusal.inc" "$S/app/tests/"
cp "$R/app/paint/painter-mypaint-surface/tests/precision-refusal.cpp" "$S/app/paint/painter-mypaint-surface/tests/"
python3 - <<'PY'
from pathlib import Path
r=Path('<checkout>/app/paint/painter-mypaint-surface/meson.build');s=Path('<native>/source/app/paint/painter-mypaint-surface/meson.build')
text=s.read_text();mark='# Explicit refusal before native pixels/profile precision can be reinterpreted.'
if mark not in text:s.write_text(text+'\n'+r.read_text().split(mark)[1].join([mark,'']) if False else text+'\n'+mark+r.read_text().split(mark,1)[1])
PY
meson setup --reconfigure "$B" "$S" >"$E/reconfigure.log" 2>&1
meson compile -C "$B" -j4 painter-mypaint-precision painter-mypaint-precision-refusal painter-mypaint-native-precision >"$E/native-build.log" 2>&1
python3 "$S/tools/run-painter-xvfb.py" meson test -C "$B" --no-rebuild --num-processes 1 --print-errorlogs --logbase precision-refusal-native painter-mypaint-precision painter-mypaint-precision-refusal painter-mypaint-native-precision >"$E/native.log" 2>&1
