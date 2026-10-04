#!/usr/bin/env bash
set -uo pipefail
cd /workspace/scratch/5b5281e79681/gimp-painter-installed-filter
source /workspace/scratch/5b5281e79681/gimp-build-restoration/env.sh
source tools/linux-debian13-env.sh
run=/workspace/scratch/5b5281e79681/filter-progress-validation/manual
exec 9>/workspace/shared/gimp-painter-build.lock
flock 9
sha256sum build-installed-filter/app/gimp-3.0 build-installed-filter/app/gimp-painter-filter-worker > "$run/binary-sha256.txt"
export GIMP3_DIRECTORY="$run/profile" GIMP3_CACHEDIR="$run/cache" GIMP3_TEMPDIR="$run/tmp"
export GIMP3_DATADIR="$run/data" GIMP3_SYSCONFDIR="$PWD/build-installed-filter/etc"
export GIMP_TESTING_PLUGINDIRS="$PWD/build-installed-filter/plug-ins/common"
export GIMP_TESTING_MENUS_PATH="$PWD/menus:$PWD/build-installed-filter/menus"
export GIMP_TESTING_INTERPRETER_DIRS="$PWD/build-installed-filter/data/interpreters"
export GIMP_TESTING_ENVIRON_DIRS="$PWD/build-installed-filter/data/environ"
export GSETTINGS_BACKEND=memory
ulimit -c 0
printf 'LAUNCH %s\n' "$(date -u +%FT%TZ)" > "$run/session.txt"
build-installed-filter/app/gimp-3.0 --new-instance --no-data --no-fonts --no-splash "$run/progress-fixture.png" > "$run/app.stdout" 2> "$run/app.stderr"
result=$?
printf 'EXIT %s %s\n' "$result" "$(date -u +%FT%TZ)" >> "$run/session.txt"
printf 'PROGRESS_MANUAL_EXIT=%s\n' "$result"
exit "$result"
