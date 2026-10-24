#!/bin/bash
# Run inside tools/run-painter-xvfb.py with the pinned build runtime env sourced.
set -euo pipefail
: "${SOURCE:?Set source directory}" "${BUILD:?Set native build directory}" "${OUT:?Set a new output directory}"
case_name=${1:?pristine-absent, native-session-absent, or explicit-false}
case "$case_name" in pristine-absent|native-session-absent|explicit-false);; *)exit 2;;esac
case_dir="$OUT/$case_name"
if test -e "$case_dir"; then echo "Refusing to overwrite $case_dir" >&2; exit 2; fi
mkdir -p "$case_dir/profile"
case "$case_name" in
 native-session-absent) cp "$SOURCE/etc/sessionrc" "$case_dir/profile/sessionrc";;
 explicit-false) printf '(painter-canvas-ui no)\n' > "$case_dir/profile/gimprc";;
esac
python3 - "$case_dir" <<'PY'
import sys,pathlib,json,hashlib
p=pathlib.Path(sys.argv[1]);files={}
for f in (p/'profile').iterdir():
 files[f.name]={'contents':f.read_text(),'sha256':hashlib.sha256(f.read_bytes()).hexdigest()}
(p/'inputs.json').write_text(json.dumps({'initial_files':files},indent=2)+'\n')
PY
unset GIMP_TESTING_ABS_TOP_SRCDIR GIMP_TESTING_ABS_TOP_BUILDDIR GIMP_TESTING_PLUGINDIRS GIMP_TESTING_SESSIONRC_NAME UI_TEST
export GIMP3_DIRECTORY="$case_dir/profile"
"$BUILD/app/gimp-3.0" --new-instance --no-splash "$SOURCE/app/tests/files/gimp-2-6-file.xcf" >"$case_dir/app.log" 2>&1 &
pid=$!
trap 'kill -TERM "$pid" 2>/dev/null || true' EXIT
sleep 10
kill -0 "$pid"
"${CAPTURE_HELPER:?Set compiled capture-x11 helper}" "$case_dir/screen.png" >"$case_dir/windows.log" 2>&1
kill -TERM "$pid"
set +e
wait "$pid"
printf '%s\n' "$?" >"$case_dir/termination.status"
trap - EXIT
exit 0
