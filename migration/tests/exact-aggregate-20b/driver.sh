#!/bin/bash
set -euo pipefail
R=/workspace/scratch/0015fc5d1a03/gimp-painter
A=/workspace/shared/gimp-clean-aggregate
exec 9>"$A/build.lock"; flock 9
sha=$(git -C "$R" rev-parse 20b2204b11)
E="$A/exact-$sha"; mkdir -p "$E"
backup="$A/source.previous-$sha"
test ! -e "$backup"
mv "$A/source" "$backup"; mkdir "$A/source"
git -C "$R" archive "$sha" | tar -x -C "$A/source"
cp -a "$backup/gimp-data/." "$A/source/gimp-data/"
printf '%s\n' "$sha" > "$E/source-commit.txt"
python3 - "$R" "$A/source" "$sha" "$E" <<'PY'
import hashlib,json,os,pathlib,subprocess,sys
r,s,sha,e=sys.argv[1:];s=pathlib.Path(s);e=pathlib.Path(e);records={};gitlinks={}
for raw in subprocess.check_output(['git','-C',r,'ls-tree','-rz','--full-tree',sha]).split(b'\0'):
 if not raw:continue
 meta,name=raw.split(b'\t',1);mode,kind,oid=meta.split();name=os.fsdecode(name)
 if kind==b'commit':gitlinks[name]=oid.decode();continue
 p=s/name;data=os.fsencode(os.readlink(p))if mode==b'120000'else p.read_bytes()
 actual=hashlib.sha1(b'blob '+str(len(data)).encode()+b'\0'+data).hexdigest()
 assert actual==oid.decode(),name
 records[name]=hashlib.sha256(data).hexdigest()
held=['app/widgets/gimpcellrendererpopup.cpp','app/display/gimppaintercanvasui.cpp','app/core/gimppaintermybrush.cpp','app/core/gimpviewable.c','app/widgets/gimpviewrenderer.c','app/tools/gimpfillbrushtool.cpp','app/tools/gimpbucketfillbrushtool.h','app/tools/gimp-tools.c','app/core/gimpclonelayer.cpp']
(e/'prebuild-tracked-source-sha256.json').write_text(json.dumps(records,indent=2)+'\n')
(e/'held-exclusions.json').write_text(json.dumps({'source_commit':sha,'scope':'Exact committed files, no worktree candidate overlays','files':{p:records.get(p,'ABSENT')for p in held}},indent=2)+'\n')
assets={str(p.relative_to(s/'gimp-data')):hashlib.sha256(p.read_bytes()).hexdigest()for p in sorted((s/'gimp-data').rglob('*'))if p.is_file()}
(e/'external-data-snapshot.json').write_text(json.dumps({'declared_gitlinks':gitlinks,'scope':'Existing dependency asset snapshot byte-pinned; repository gitlink commit identity not inferred solely from these hashes','files':assets},indent=2)+'\n')
print('Exact tracked source verified:',len(records),'files; external data snapshot:',len(assets),'files')
PY
cp "$E/source-commit.txt" "$A/source-commit.txt"
source "$A/env.sh"
meson setup --reconfigure "$A/build" "$A/source" > "$E/reconfigure.log" 2>&1
# Explicit targets only; never the held candidate targets.
targets=(prepared-undo undo-pop-guard painter-prepared-pixels painter-stroke-undo paired-stroke-undo paired-stroke-ui mypaint-options-construction mypaint-options-construction-reentry gimp-painter-surface painter-mypaint-tool painter-mypaint-work-buffer feature-lifecycle preset-gui-entries application-owned-lifecycle painter-layer-modes painter-mode-ui layer-presets-ui mypaint-undo-ref mypaint-options-gui painter-graph-invalidation painter-mypaint-resource)
printf '%s\n' "${targets[@]}" > "$E/selected-targets.txt"
meson compile -C "$A/build" -j2 "${targets[@]}" gimp-filter-layer > "$E/build.log" 2>&1
mkdir -p "$E/options-gui-screenshots" "$E/filter-flip-screenshots"
export GIMP_OPTIONS_GUI_SCREENSHOT_DIR="$E/options-gui-screenshots"
export GIMP_TEST_FILTER_FLIP_SCREENSHOT_DIR="$E/filter-flip-screenshots"
test_status=0
python3 "$A/source/tools/run-painter-xvfb.py" meson test -C "$A/build" --no-rebuild --num-processes 1 --print-errorlogs --logbase exact-committed-aggregate "${targets[@]}" > "$E/tests.log" 2>&1 || test_status=$?
printf '%s\n' "$test_status" > "$E/test-command-status.txt"
python3 - "$A/build/meson-logs/exact-committed-aggregate.json" "$E/results.json" <<'PY'
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
