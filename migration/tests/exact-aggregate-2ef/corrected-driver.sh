#!/bin/bash
set -euo pipefail
A=/workspace/shared/gimp-clean-aggregate
exec 9>"$A/build.lock"; flock 9
source "$A/env.sh"
E="$A/exact-2ef2dd32eeb7201e8e0318f169c6005a21c10410"
mapfile -t targets < "$E/selected-targets.txt"
cp "$E/tests.log" "$E/first-runner-tests.log"
cp "$E/test-command-status.txt" "$E/first-runner-status.txt"
mkdir -p "$E/options-gui-screenshots" "$E/filter-flip-screenshots" "$E/filter-geometry-screenshots"
export GIMP_TEST_FILTER_GEOMETRY_SCREENSHOT_DIR="$E/filter-geometry-screenshots"
export GIMP_OPTIONS_GUI_SCREENSHOT_DIR="$E/options-gui-screenshots"
export GIMP_TEST_FILTER_FLIP_SCREENSHOT_DIR="$E/filter-flip-screenshots"
test_status=0
python3 "$A/source/tools/run-painter-xvfb.py" meson test -C "$A/build" --no-rebuild --num-processes 1 --print-errorlogs --logbase exact-committed-aggregate-corrected "${targets[@]}" painter-perspective-ui-geometry painter-perspective-ui-damage-0 painter-perspective-ui-damage-1 painter-perspective-ui-damage-2 > "$E/tests.log" 2>&1 || test_status=$?
printf '%s\n' "$test_status" > "$E/test-command-status.txt"
python3 - "$A/build/meson-logs/exact-committed-aggregate-corrected.json" "$E/results.json" <<'PY'
import json,sys
rows=[json.loads(x)for x in open(sys.argv[1])]
json.dump([{k:r.get(k)for k in ['name','result','returncode','duration','stdout','stderr','command']}for r in rows],open(sys.argv[2],'w'),indent=2)
# Record failures without skipping the post-run source seal.
PY
python3 - "$A/source" "$E" <<'PYSEAL'
import hashlib,json,os,pathlib,sys
s,e=map(pathlib.Path,sys.argv[1:]);before=json.load(open(e/'prebuild-tracked-source-sha256.json'));changed=[]
for n,h in before.items():
 p=s/n;data=os.fsencode(os.readlink(p))if p.is_symlink()else p.read_bytes()
 if hashlib.sha256(data).hexdigest()!=h:changed.append(n)
(e/'postrun-source-seal.json').write_text(json.dumps({'changed':changed,'checked_files':len(before)},indent=2)+'\n')
assert not changed,changed
PYSEAL
test "$test_status" -eq 0
printf 'PASS\n' > "$E/result.txt"
