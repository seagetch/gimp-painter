#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Run native integration tests, including ephemeral loopback only.
set -uo pipefail
root="$(cd "$(dirname "$0")/../.." && pwd)"
build="$(realpath "${1:-$root/build-debian13}")"
mode="${2:-normal}"
case "$mode" in normal) suffix=;; asan) suffix=-asan;; *) exit 2;; esac
cd "$root" || exit 2
exec 9>"${GIMP_PAINTER_BUILD_LOCK:-/workspace/shared/gimp-painter-build.lock}"; flock 9
if [[ "$mode" == normal ]]; then
  ninja -C "$build" app/tests/painter-http app/tests/painter-http-ui || exit $?
fi
export GIMP_TESTING_ABS_TOP_SRCDIR="$root" GIMP_TESTING_ABS_TOP_BUILDDIR="$build"
export GIMP_TESTING_PLUGINDIRS="$build/plug-ins/common" UI_TEST=yes
export ASAN_OPTIONS=detect_leaks=0:halt_on_error=1:abort_on_error=1
export GSETTINGS_BACKEND=memory
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
unset GIMP_PAINTER_HTTP_ENABLE GIMP_PAINTER_HTTP_TOKEN GIMP_PAINTER_HTTP_PORT GIMP_PAINTER_HTTP_WEBHOOK_ORIGIN
if [[ "$mode" == asan ]]; then
  python3 - "$build/httpd-sanitizer-build.json" <<'VERIFY'
import hashlib,json,pathlib,sys,tarfile
report=json.loads(pathlib.Path(sys.argv[1]).read_text())
assert not report['changed_during_build']
with tarfile.open('migration/tests/httpd-tested-sources.tar.gz') as archive:
    for path,digest in report['sources_sha256'].items():
        assert hashlib.sha256(archive.extractfile(path).read()).hexdigest()==digest,path
        # Other migration work may advance a compatibility-only module after
        # this immutable build. Own HTTP implementation/tests must stay exact.
        if path.startswith('app/httpd/') or path.startswith('app/tests/test-painter-http'):
            assert hashlib.sha256(pathlib.Path(path).read_bytes()).hexdigest()==digest,path
for path,digest in report['executables_sha256'].items():
    assert hashlib.sha256(pathlib.Path(path).read_bytes()).hexdigest()==digest,path
VERIFY
  [[ $? == 0 ]] || exit 2
fi
{
  printf 'Native HTTP mode=%s display=%s; all listeners ephemeral loopback\n' "$mode" "${DISPLAY:-unset}"
  "$build/app/tests/painter-http$suffix"; native=$?
  "$build/app/tests/painter-http-ui$suffix"; gui=$?
  printf 'PAINTER_HTTP_NATIVE_EXIT=%s GUI_EXIT=%s\n' "$native" "$gui"
  [[ "$native" == 0 && "$gui" == 0 ]]
} 2>&1 | tee "$build/painter-http-$mode.log"
exit "${PIPESTATUS[0]}"
