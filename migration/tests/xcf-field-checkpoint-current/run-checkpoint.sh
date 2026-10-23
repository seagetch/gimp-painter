#!/bin/bash
set -euo pipefail
source /workspace/shared/gimp-native-baseline/env.sh
exec 9>/workspace/shared/gimp-painter-build.lock
flock 9
R=/workspace/scratch/0015fc5d1a03/gimp-painter
S=/workspace/shared/gimp-native-baseline/source
B=/workspace/shared/gimp-native-baseline/build-port
E=/workspace/shared/xcf-field-checkpoint
cp "$R/migration/tests/run_xcf_field_checkpoint_sanitizers.py" "$S/migration/tests/"
ninja -C "$B" app/tests/xcf app/tests/painter-provenance app/tests/painter-xcf-open app/tests/painter-xcf-roundtrip >"$E/normal-build.log" 2>&1
python3 "$S/tools/run-painter-xvfb.py" meson test -C "$B" --no-rebuild --print-errorlogs xcf painter-provenance painter-xcf-open painter-xcf-roundtrip >"$E/normal.log" 2>&1
python3 - "$B" "$E" <<'PY'
import json,pathlib,sys
b,e=map(pathlib.Path,sys.argv[1:]);rows=[json.loads(x)for x in (b/'meson-logs/testlog.json').read_text().splitlines()];assert len(rows)==4 and all(x['result']=='OK'for x in rows)
keys=['name','result','returncode','duration','stdout','stderr','command']
(e/'normal.json').write_text(json.dumps([{k:r.get(k,'')for k in keys}for r in rows],indent=2)+'\n')
PY
python3 "$S/tools/run-painter-xvfb.py" python3 "$S/migration/tests/run_xcf_field_checkpoint_sanitizers.py" "$B" --report "$E/sanitizers.json" --seal-inputs "$E/tested-sources.tar.gz" --run >"$E/sanitizers.log" 2>&1
