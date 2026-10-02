#!/usr/bin/env bash
# Rebuild the Linux reference from clean pinned checkouts; no host installation.
set -euo pipefail
if [ "$#" -ne 5 ]; then
  echo "usage: $0 DEPS_DIRECTORY BUILD_DIRECTORY GIMP_SOURCE BABL_SOURCE GEGL_SOURCE" >&2
  exit 2
fi
repo=$(cd "$(dirname "$0")/.." && pwd)
deps=$(realpath "$1")
state=$(realpath -m "$2")
gimp_source=$(realpath "$3")
babl_source=$(realpath "$4")
gegl_source=$(realpath "$5")
check_source() {
  [ "$(git -C "$1" rev-parse HEAD)" = "$2" ] || { echo "wrong source revision: $1" >&2; exit 1; }
  git -C "$1" diff --quiet || { echo "use a clean tracked source checkout: $1" >&2; exit 1; }
  git -C "$1" diff --cached --quiet || { echo "source index has changes: $1" >&2; exit 1; }
}
check_source "$gimp_source" afa43fae3e920210146abed514f136fd49f671b5
check_source "$babl_source" e92ced2c54250af1dddb58f76ff1678d74a30dcf
check_source "$gegl_source" 29254dcd875d271043aba57d0e6ebc48ec63c013
python3 "$repo/tools/prepare_legacy_build_env.py" --deps "$deps" --output "$state"
# Generated environment deliberately tolerates an unset inherited library path.
set +u
source "$state/env.sh"
set -u
jobs=${JOBS:-4}
unset CFLAGS CXXFLAGS
export CC=/usr/bin/gcc CXX=/usr/bin/g++
run() { local log=$1; shift; printf '%q ' "$@" > "$state/logs/$log.command"; printf '\n' >> "$state/logs/$log.command"; "$@" > "$state/logs/$log.log" 2>&1; }
cd "$babl_source"
run babl-bootstrap autoreconf --force --install --verbose
run babl-configure ./configure --prefix="$state/prefix" --disable-docs
run babl-build make -j"$jobs"
run babl-install make install
run babl-tests-isolated make -C tests check -j"$jobs" "TESTS_ENVIRONMENT=LD_LIBRARY_PATH=$state/prefix/lib:$GIMP_DEPS_ROOT/usr/lib/x86_64-linux-gnu BABL_PATH=$babl_source/extensions/.libs"
cd "$gegl_source"
run gegl-bootstrap autoreconf --force --install --verbose
export CFLAGS='-g -O3 -mmmx -msse -ftree-vectorize -ffast-math -fcommon'
run gegl-configure ./configure --prefix="$state/prefix" --disable-docs --disable-introspection --without-libavformat --without-libspiro --without-openexr --without-sdl --without-librsvg --without-libraw --without-gexiv2 --without-exiv2 --without-lua --without-lensfun --without-vala
run gegl-build make -j"$jobs"
run gegl-install make install
# Preserve the known failure rather than silently declaring dependency tests green.
set +e
run gegl-tests-simple make -C tests/simple check -j"$jobs"
gegl_test_status=$?
set -e
printf '%s\n' "$gegl_test_status" > "$state/logs/gegl-tests-simple.exit-code"
unset CFLAGS
cd "$gimp_source"
for patch in "$repo"/migration/baseline/legacy/patches/gimp-*.patch; do
  git apply --check "$patch"
  git apply "$patch"
done
run gimp-bootstrap env NOCONFIGURE=1 ./autogen.sh --disable-gtk-doc
export CFLAGS='-g -O2 -fcommon -Wno-error=incompatible-pointer-types'
export CXXFLAGS='-g -O2 -include type_traits'
run gimp-configure ./configure --prefix="$state/prefix" --disable-gtk-doc --disable-python --without-webkit --without-poppler --without-librsvg --without-libmng --without-aa --without-wmf --without-libjasper --without-print --without-gudev --without-dbus --without-alsa --without-gvfs --disable-httpd
run gimp-build make -j"$jobs"
run gimp-install make -j"$jobs" install
run gimp-version "$state/prefix/bin/gimp-2.8" --version
sha256sum "$state/prefix/bin/gimp-2.8" > "$state/logs/gimp-binary.sha256"
echo "Legacy binary built. GEGL simple-test exit status: $gegl_test_status. Review logs; no runtime compatibility or GUI pass is implied."
