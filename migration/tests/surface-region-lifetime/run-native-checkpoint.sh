#!/bin/bash
set -euo pipefail
source /workspace/shared/gimp-native-baseline/env.sh
exec 9>/workspace/shared/gimp-painter-build.lock
flock 9
R=/workspace/scratch/0015fc5d1a03/gimp-painter
S=/workspace/shared/gimp-native-baseline/source
B=/workspace/shared/gimp-native-baseline/build-port
E=/workspace/shared/surface-region-lifetime-final
for f in app/paint/painter-mypaint-surface/meson.build app/paint/painter-mypaint-surface/tests/region-lifetime.cpp app/paint/painter-mypaint-surface/tests/region-lifetime-internal.inc; do cp "$R/$f" "$S/$f"; done
meson setup --reconfigure "$B" "$S" >"$E/native-configure.log" 2>&1
ninja -C "$B" app/paint/painter-mypaint-surface/painter-mypaint-region-internals app/paint/painter-mypaint-surface/painter-mypaint-region-lifetime >"$E/native-build.log" 2>&1
python3 "$S/tools/run-painter-xvfb.py" meson test -C "$B" --no-rebuild --print-errorlogs painter-mypaint-region-internals painter-mypaint-region-lifetime >"$E/native.log" 2>&1
python3 - "$B" "$E" <<'PY'
import json,pathlib,sys
b,e=map(pathlib.Path,sys.argv[1:]);rows=[json.loads(x)for x in (b/'meson-logs/testlog.json').read_text().splitlines()];assert len(rows)==2 and all(x['result']=='OK'for x in rows)
keys=['name','result','returncode','duration','stdout','stderr','command']
(e/'native.json').write_text(json.dumps([{k:r.get(k,'')for k in keys}for r in rows],indent=2)+'\n')
print('PASS native region internals14 + existing11')
PY
