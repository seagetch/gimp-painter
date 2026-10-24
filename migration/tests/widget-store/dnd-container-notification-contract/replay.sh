#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
root="$(cd "$(dirname "$0")/../../../.." && pwd)"
build="$(realpath "${1:?usage: replay.sh BUILD_DIRECTORY OUTPUT_DIRECTORY}")"
out="$(realpath -m "${2:?OUTPUT_DIRECTORY required}")"
if [[ -n "${GIMP_DEPS_DIRECTORY:-}" ]]; then
  source "$root/tools/linux-debian13-env.sh"
fi
mkdir -p "$out"/{home,config,data,cache,tmp}
export HOME="$out/home" XDG_CONFIG_HOME="$out/config" XDG_DATA_HOME="$out/data"
export XDG_CACHE_HOME="$out/cache" TMPDIR="$out/tmp" GSETTINGS_BACKEND=memory UI_TEST=yes
export GIMP_TESTING_ABS_TOP_SRCDIR="$root" GIMP_TESTING_ABS_TOP_BUILDDIR="$build"
export GIMP_TESTING_PLUGINDIRS="$build/plug-ins/common" GIMP_TEST_PAINTER_CANVAS_GROUP=original
unset GIMP_TEST_DOCK_CONTENT_SCREENSHOTS
# Raw build output remains private (it may include inherited environment).
ninja -C "$build" app/tests/painter-canvas-ui menus/image-menu.ui menus/dockable-menu.ui > "$out/build.private.log" 2>&1
python3 "$root/tools/run-painter-xvfb.py" python3 - "$build" "$out" <<'PY'
import json,pathlib,re,subprocess,sys,time
build,out=map(pathlib.Path,sys.argv[1:]); t=time.monotonic()
r=subprocess.run([str(build/'app/tests/painter-canvas-ui')],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=200)
(out/'run.private.log').write_text(r.stdout)
passed=re.findall(r'^ok\s+\d+\s+(/\S+)',r.stdout,re.M)
failed=re.findall(r'^not ok (?:\d+ )?(/\S+)',r.stdout,re.M)
skips=[l for l in r.stdout.splitlines() if '# SKIP' in l]
result={'exit_status':r.returncode,'duration_seconds':time.monotonic()-t,'passed_cases':passed,'failed_cases':failed,'skips':skips}
(out/'results.json').write_text(json.dumps(result,indent=2)+'\n')
if r.returncode or len(passed)!=107 or failed or skips:
    raise SystemExit('FAIL: inspect private run log and compact results')
print('PASS: all 107 original canvas cases, no skips/exclusions')
PY
