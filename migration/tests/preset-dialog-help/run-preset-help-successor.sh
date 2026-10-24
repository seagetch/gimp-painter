#!/bin/bash
set -euo pipefail
R=/workspace/scratch/0015fc5d1a03/gimp-painter;W=/workspace/shared/gimp-clean-aggregate;E=/workspace/shared/preset-help-successor
exec 9>"$W/build.lock";flock 9
source "$W/env.sh"
mkdir -p "$E/red" "$E/green"
python3 - "$R" "$W/source" "$E" <<'PY'
from pathlib import Path
import json,hashlib,sys,tarfile,subprocess
r,s,e=map(Path,sys.argv[1:]);sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();m=json.load(open('/workspace/shared/options-readiness/green/source-manifest.json'));expected=m['complete_source_sha256'];f=json.load(open('/workspace/shared/filter-property-proof/source-after.json'));expected.update(f['sha256']);assert all(sha(s/p)==h for p,h in expected.items());(e/'predecessor.json').write_text(json.dumps({'base':m['base'],'readiness_sha256':sha(Path('/workspace/shared/options-readiness/green/source-manifest.json')),'filter_sha256':sha(Path('/workspace/shared/filter-property-proof/source-after.json')),'complete_source_sha256':expected},indent=2));(e/'baseline-dialogs.c').write_bytes((s/'app/dialogs/dialogs.c').read_bytes());(s/'app/tests/test-layer-presets-ui.c').write_bytes((r/'app/tests/test-layer-presets-ui.c').read_bytes())
PY
seal() {
python3 - "$W/source" "$E/$1" <<'PY'
from pathlib import Path
import sys,json,hashlib,tarfile
s,e=map(Path,sys.argv[1:]);paths=['app/dialogs/dialogs.c','app/tests/test-layer-presets-ui.c','app/tests/tests.c','app/tests/meson.build','app/dialogs/preferences-dialog.c','app/actions/dialogs-actions.c','app/widgets/gimpdockable.c','app/widgets/gimpdockbook.c','app/widgets/gimphelp.c','libgimpwidgets/gimphelpui.c','app/tests/test-preset-gui-entries.c'];sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();m={'scope':'one-field dialog help successor over exact2ef + sealed readiness + Filter property overlays','owned_sha256':{p:sha(s/p) for p in paths[:2]},'input_sha256':{p:sha(s/p) for p in paths},'complete_source_sha256':{str(p.relative_to(s)):sha(p) for p in s.rglob('*') if p.is_file() and '__pycache__' not in p.parts and '.git' not in p.parts}};(e/'source-manifest.json').write_text(json.dumps(m,indent=2))
with tarfile.open(e/'tested-inputs.tar.gz','w:gz') as t:
 for p in paths:t.add(s/p,arcname=p)
PY
}
results() {
python3 - "$W/build/meson-logs/$1.json" "$E/$2/results.json" <<'PY'
import json,sys
json.dump([{k:r.get(k) for k in ['name','result','returncode','stdout','stderr','duration','command']} for r in map(json.loads,open(sys.argv[1]))],open(sys.argv[2],'w'),indent=2)
PY
}
seal red
ninja -C "$W/build" -j2 app/tests/layer-presets-ui >"$E/red/build.log" 2>&1
set +e
python3 "$W/source/tools/run-painter-xvfb.py" meson test -C "$W/build" layer-presets-ui --no-rebuild --print-errorlogs --test-args='-p /layer-presets-ui/dialog-factory-dockable-lifetime' --logbase preset-help-red >"$E/red/portrait.log" 2>&1
status=$?
set -e
results preset-help-red red
[ "$status" -ne 0 ]
python3 - "$E/red/results.json" <<'PY'
import json,sys
v=json.load(open(sys.argv[1]))[0];assert 'entry->help_id' in v['stdout']+v['stderr'];assert 'gimp-dock' in v['stdout']+v['stderr']
PY
cp "$R/app/dialogs/dialogs.c" "$W/source/app/dialogs/dialogs.c"
seal green
ninja -C "$W/build" -j2 app/tests/layer-presets-ui app/tests/preset-gui-entries >"$E/green/build.log" 2>&1
python3 "$W/source/tools/run-painter-xvfb.py" meson test -C "$W/build" layer-presets-ui preset-gui-entries --no-rebuild --print-errorlogs --logbase preset-help-green >"$E/green/portrait.log" 2>&1
results preset-help-green green
python3 - "$W/source" "$E/green" <<'PY'
from pathlib import Path
import json,hashlib,sys
s,e=map(Path,sys.argv[1:]);m=json.load(open(e/'source-manifest.json'));assert all(hashlib.sha256((s/p).read_bytes()).hexdigest()==h for p,h in m['complete_source_sha256'].items());(e/'source-after.json').write_text(json.dumps({'complete_recorded_source_unchanged':True}))
PY
