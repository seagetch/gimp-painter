#!/usr/bin/env bash
# Hosted normal assertion control only; never a candidate success or leak waiver.
set -euo pipefail
here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
src=$(realpath "${1:?source required}"); build=$(realpath "${2:?build required}")
run=$(realpath "${3:?private CI scratch required}"); out=$(realpath "${4:?artifact directory required}")
[[ ${GITHUB_ACTIONS:-false} == true ]] || { echo 'BLOCKED: ordinary hosted Actions only' >&2; exit 2; }
: "${BRUSH_CI_RUNTIME_PINS:?automatic source seal required}"
python3 -B - "$run" "${RUNNER_TEMP:?}" <<'PY'
import os,pathlib,re,stat,sys
run,root=map(pathlib.Path,sys.argv[1:]); info=run.lstat()
assert run.parent==root.resolve() and re.fullmatch(r'painter-brush-ci\.[A-Za-z0-9]{8}',run.name)
assert stat.S_ISDIR(info.st_mode) and not run.is_symlink() and info.st_uid==os.getuid() and info.st_mode&0o077==0
PY
evidence="$out/canvas-old-provider-red"
mkdir "$evidence"
python3 -B "$here/canvas_ownership_red.py" --provider "$build/app/tests/color-ownership/canvas-ownership-old-provider.map" \
  --source "$src" --build "$build" --output "$evidence/provider.json"
failed=0
for leaf in unrealized-padding realized-padding; do
  label="canvas-old-$leaf"; profile="$run/profile-$label"
  mkdir -p "$profile"/{home,config,cache,data,state,runtime,tmp,gimp-temp,gimp-cache}
  chmod 700 "$profile/runtime" "$profile/tmp" "$profile/gimp-temp" "$profile/gimp-cache"
  status=0
  (
    export HOME="$profile/home" XDG_CONFIG_HOME="$profile/config" XDG_CACHE_HOME="$profile/cache"
    export XDG_DATA_HOME="$profile/data" XDG_STATE_HOME="$profile/state" XDG_RUNTIME_DIR="$profile/runtime"
    export TMPDIR="$profile/tmp" GIMP3_TEMPDIR="$profile/gimp-temp" GIMP3_CACHEDIR="$profile/gimp-cache"
    export GDK_BACKEND=x11 GDK_SCALE=1 GDK_DPI_SCALE=1
    unset DISPLAY WAYLAND_DISPLAY GIMP_TESTING_SESSIONRC_NAME GSETTINGS_BACKEND GIMP3_DIRECTORY NO_AT_BRIDGE
    xvfb-run --auto-servernum --server-args='-screen 0 1080x1920x24 -nolisten tcp' \
      dbus-run-session -- meson test -C "$build" --no-rebuild --num-processes 1 \
        --print-errorlogs --logbase "$label" --test-args "-p /ownership/canvas/$leaf" canvas-ownership-old-provider
  ) > "$run/$label-console.log" 2>&1 || status=$?
  # Only this historical normal control uses Meson should_fail. Its expected
  # failure still requires exact child SIGABRT/assertion, never any nonzero.
  if [[ "$status" != 0 ]]; then failed=1; fi
  python3 -B "$here/canvas_ownership_red.py" --log "$build/meson-logs/$label.json" \
    --case-id "/ownership/canvas/$leaf" --output "$evidence/$leaf.json" || failed=1
  python3 -B - "$run/$label-console.log" "$evidence/$leaf-console.log" <<'PY'
import pathlib,re,sys
lines=pathlib.Path(sys.argv[1]).read_text(errors='replace').splitlines()
safe=[x for x in lines if not re.match(r'^\s*(?:>>>|Inherited environment:|Environment:|[A-Za-z_][A-Za-z_0-9]*=)',x)]
pathlib.Path(sys.argv[2]).write_text('\n'.join(safe)+'\n')
PY
done
python3 -B "$here/canvas_ownership_red.py" --aggregate "$evidence/unrealized-padding.json" "$evidence/realized-padding.json" \
  --provider-receipt "$evidence/provider.json" --output "$evidence/aggregate.json" || failed=1
exit "$failed"
