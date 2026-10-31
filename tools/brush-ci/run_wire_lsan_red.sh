#!/usr/bin/env bash
# Separate C5 library/test build; expected LSan RED is never candidate PASS.
set -euo pipefail
here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
repo=$(realpath "${1:?owned candidate checkout required}")
run=$(realpath "${2:?owned CI scratch required}")
out=$(realpath "${3:?artifact directory required}")
[[ ${GITHUB_ACTIONS:-false} == true ]] || exit 2
pins=${BRUSH_CI_RUNTIME_PINS:?automatic runtime seal required}
python3 -B - "$run" "${RUNNER_TEMP:?}" <<'PYOWNED'
import os,pathlib,re,stat,sys
run,root=map(pathlib.Path,sys.argv[1:])
info=run.lstat()
assert run.parent==root.resolve() and re.fullmatch(r'painter-brush-ci\.[A-Za-z0-9]{8}',run.name)
assert stat.S_ISDIR(info.st_mode) and not run.is_symlink() and info.st_uid==os.getuid() and info.st_mode&0o077==0
PYOWNED
source="$run/wire-red-source"; build="$run/wire-red-build"; private="$run/wire-red-runtime"
evidence="$out/wire-lsan-red"; stage=source; result=0
mkdir "$evidence"; mkdir -m 700 "$private"
finish() {
  result=$?; trap - EXIT
  if [[ -f "$evidence/source-overlay.json" ]]; then
    python3 -B "$here/wire_lsan_red.py" --source "$source" --verify "$evidence/source-overlay.json" \
      > "$evidence/post-run-source-integrity.log" 2>&1 || result=1
  fi
  printf 'stage=%s\nexit_code=%s\nnegative_control=true\nacceptance_eligible=false\n' "$stage" "$result" > "$evidence/status.txt"
  exit "$result"
}
trap finish EXIT
# C5 is no longer an ancestor after task amendments: fetch its immutable public
# SHA explicitly. Never rely on local dangling history in a depth2 checkout.
old=da070df3717723e75f0757113c2da2768949d676
git -C "$repo" fetch --no-tags --depth=1 origin "$old"
git -C "$repo" worktree add --detach "$source" "$old"
test "$(git -C "$source" rev-parse HEAD^{tree})" = 544a91ca352614a58f024d1e3cd813ae0226df31
git -C "$source" -c submodule.gimp-data.url=https://github.com/GNOME/gimp-data.git \
  submodule update --init --depth=1 gimp-data
test "$(git -C "$source/gimp-data" rev-parse HEAD)" = "$(git -C "$source" rev-parse HEAD:gimp-data)"
python3 -B "$here/wire_lsan_red.py" --source "$source" --candidate "$repo" --pins "$pins" \
  --output "$evidence/source-overlay.json"
# Reuse only the parent's unchanged signed/locked dependency preparation. This
# is a separate ordinary GIMP Meson provider build, never hand-picked .o files.
{
  sha256sum "$source/migration/baseline/debian13-package-lock.json" "$repo/migration/baseline/debian13-package-lock.json"
  pkg-config --modversion glib-2.0 gio-2.0 gegl-0.4 babl-0.1
  meson --version; ninja --version; gcc --version | head -n 1
} > "$evidence/dependency-identity.txt"
stage=configure
meson setup "$build" "$source" --prefix="$run/wire-red-prefix" \
  --buildtype=debugoptimized -Dauto_features=disabled -Dlibunwind=false \
  -Dheadless-tests=disabled -Dpainter-http=disabled \
  -Denable-console-bin=false -Dgi-docgen=disabled -Dappdata-test=disabled \
  -Djavascript=disabled -Dvala=disabled -Dprint=false -Dcheck-update=no \
  -Dlibbacktrace=false -Db_sanitize=address,undefined 2>&1 | tee "$evidence/configure.log"
stage=compile
meson compile -C "$build" -j 2 libgimpbase/test-choice-lifecycle:executable \
  2>&1 | tee "$evidence/compile.log"
cp "$build/compile_commands.json" "$evidence/compile-commands.json"
ninja -C "$build" -t commands libgimpbase/test-choice-lifecycle > "$evidence/build-commands.txt"
python3 -B - "$build" "$evidence/build-provider.json" <<'PYBUILD'
import hashlib,json,pathlib,re,shlex,subprocess,sys
build=pathlib.Path(sys.argv[1]).resolve()
options=json.loads(subprocess.check_output(['meson','introspect','--buildoptions',str(build)]))
selected=[x for x in options if x['name']=='b_sanitize'];assert len(selected)==1
value=selected[0]['value'];assert set(value if isinstance(value,list)else value.split(','))=={'address','undefined'}
commands=json.loads((build/'compile_commands.json').read_text())
compiler_sources=[]
for name in ('gimpprotocol.c','gimpwire.c','gimpchoice.c','test-choice-lifecycle.c'):
 expected=(build.parent/'wire-red-source/libgimpbase'/name).resolve()
 matching=[row for row in commands if (pathlib.Path(row['directory'])/row['file']).resolve()==expected]
 assert len(matching)==1,'Missing/duplicate actual provider compiler input'
 words=matching[0].get('arguments') or shlex.split(matching[0]['command'])
 sanitizers={item for word in words if word.startswith('-fsanitize=') for item in word.split('=',1)[1].split(',')}
 assert {'address','undefined'}<=sanitizers,'Provider/test source not actually instrumented'
 compiler_sources.append({'source':'libgimpbase/'+name,'sha256':hashlib.sha256(expected.read_bytes()).hexdigest(),'sanitizers':sorted(sanitizers)})
targets=json.loads(subprocess.check_output(['meson','introspect','--targets',str(build)]))
rows=[];paths={}
for name in ('gimpbase-3.0','test-choice-lifecycle'):
 found=[t for t in targets if t['name']==name];assert len(found)==1 and len(found[0]['filename'])==1
 path=pathlib.Path(found[0]['filename'][0]);assert path.is_file() and not path.is_symlink() and path.resolve().is_relative_to(build)
 paths[name]=path.resolve();rows.append({'name':name,'file':str(path.relative_to(build)),'sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'bytes':path.stat().st_size})
linked=subprocess.check_output(['ldd',str(paths['test-choice-lifecycle'])],text=True)
provider=re.findall(r'libgimpbase-[^\s]+\s+=>\s+(\S+)',linked)
assert len(provider)==1 and pathlib.Path(provider[0]).resolve()==paths['gimpbase-3.0'],'Not the actual old-provider library'
libraries=[]
for name,path in re.findall(r'^\s*(\S+)\s+=>\s+(\S+)\s+\(',linked,re.M):
 p=pathlib.Path(path);assert p.is_file()
 libraries.append({'name':name,'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'bytes':p.stat().st_size})
assert libraries and len(libraries)<100
pathlib.Path(sys.argv[2]).write_text(json.dumps({'b_sanitize':selected,'compiler_sources':compiler_sources,'artifacts':rows,'resolved_libraries':libraries,'actual_old_provider_verified':True,'negative_control':True,'acceptance_eligible':False},indent=2)+'\n')
PYBUILD
python3 -B "$here/wire_lsan_red.py" --source "$source" --verify "$evidence/source-overlay.json"
stage=runtime
mkdir -p "$private"/{home,config,cache,data,state,tmp,runtime}
chmod 700 "$private/tmp" "$private/runtime"
status=0; gate=0
(
  unset DISPLAY WAYLAND_DISPLAY LSAN_OPTIONS
  unset GIMP_TEST_PROTOCOL_FIXTURE GIMP_TEST_PROTOCOL_SCENARIO GIMP_TEST_PROTOCOL_LOG_DIR GIMP_TEST_EXPECT_REDUNDANT_QUIT
  export HOME="$private/home" XDG_CONFIG_HOME="$private/config" XDG_CACHE_HOME="$private/cache"
  export XDG_DATA_HOME="$private/data" XDG_STATE_HOME="$private/state" XDG_RUNTIME_DIR="$private/runtime" TMPDIR="$private/tmp"
  export ASAN_OPTIONS=halt_on_error=1:detect_leaks=1:fast_unwind_on_malloc=0:malloc_context_size=40:log_exe_name=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
  python3 -B - "$evidence/runtime-options.json" <<'PYOPTIONS'
import json,os,pathlib,sys
pathlib.Path(sys.argv[1]).write_text(json.dumps({'ASAN_OPTIONS':os.environ['ASAN_OPTIONS'],'UBSAN_OPTIONS':os.environ['UBSAN_OPTIONS'],'LSAN_OPTIONS_unset':'LSAN_OPTIONS'not in os.environ,'DISPLAY_unset':'DISPLAY'not in os.environ,'WAYLAND_DISPLAY_unset':'WAYLAND_DISPLAY'not in os.environ,'raw_environment_exported':False},indent=2)+'\n')
PYOPTIONS
  meson test -C "$build" --no-rebuild --num-processes 1 --print-errorlogs \
    --logbase wire-lsan-red --test-args '-p /choice-lifecycle/wire-roundtrip' choice-lifecycle
) > "$private/console.log" 2>&1 || status=$?
python3 -B "$here/wire_lsan_red.py" --log "$build/meson-logs/wire-lsan-red.json" \
  --output "$evidence/results.json" || gate=$?
python3 -B - "$private/console.log" "$evidence/console.log" <<'PYSAFE'
import pathlib,re,sys
lines=pathlib.Path(sys.argv[1]).read_text(errors='replace').splitlines()
pathlib.Path(sys.argv[2]).write_text('\n'.join(x for x in lines if not re.match(r'^\s*(?:>>>|Inherited environment:|Environment:|[A-Za-z_][A-Za-z_0-9]*=)',x))+'\n')
PYSAFE
stage=expected-red-verification
(( status != 0 && gate == 0 ))
