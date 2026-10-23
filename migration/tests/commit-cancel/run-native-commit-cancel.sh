#!/bin/bash
set -euo pipefail
source <native>/env.sh
exec 9><shared>/gimp-painter-build.lock;flock 9
R=<checkout>
S=<native>/source
B=<native>/build-port
E=<evidence>
mkdir -p "$E"
cp "$R/app/tests/test-gimp-painter-surface.cpp" "$R/app/tests/test-painter-surface-commit-cancel.inc" "$R/app/tests/test-painter-surface-dirty-handoff.inc" "$S/app/tests/"
meson compile -C "$B" -j2 gimp-painter-surface >"$E/build.log" 2>&1
python3 "$S/tools/run-painter-xvfb.py" meson test -C "$B" --no-rebuild --num-processes 1 --print-errorlogs --logbase undo-native gimp-painter-surface >"$E/run.log" 2>&1
python3 - "$B" "$E" <<'PY'
import json,sys,pathlib
b,e=map(pathlib.Path,sys.argv[1:]);rows=[json.loads(x)for x in (b/'meson-logs/undo-native.json').read_text().splitlines()];assert len(rows)==1 and rows[0]['result']=='OK';r=rows[0];assert sum(x.startswith('ok ')for x in r['stdout'].splitlines())==22
(e/'native.json').write_text(json.dumps([{k:r.get(k)for k in ['name','result','returncode','duration','stdout','stderr','command']}],indent=2)+'\n')
PY
