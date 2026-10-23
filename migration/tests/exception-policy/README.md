# Original 04.010: compiler exception policy

This checkpoint covers the current Painter C++ exception compiler policy and
the common C boundary regression. It does not close later feature migration,
RTTI separation, platform, installation or complete C ABI audit work.

## Native compiler result

The preserved pre-change default and HTTP command snapshots are intentionally
reported as `FAIL` in `baseline-default.json` and `baseline-http.json`: 16 Painter
production `.cpp` units in each build lacked explicit exception flags. Their
standalone throw/catch/RAII probes passed under GCC 14.2 defaults, but the same
commands failed when an earlier `-fno-exceptions` modeled a global override.
These historical command snapshots are separate from the freshly regenerated
build metadata used for `default.json` and `http.json`.

After the change, all 64 default and 69 HTTP Painter production `.cpp` commands
explicitly enable exceptions and pass the earlier-disable probe. All 73 and 78
production C++ commands, including nine upstream `.cc` units, pass standalone
throw/catch and destructor-unwinding probes. App target identity comes from
Meson target metadata; fixture executables, including those compiling the same
production source files, are excluded. A final `-fno-exceptions` makes every
probe fail, checking that the fixture actually requires exception support.

`compile-policy-diff.json` shows that only 20 C++ commands per configuration
change, by adding `-fexceptions` to core/display: 16 Painter `.cpp` and four
existing core `.cc` units. All 1,865 default and 1,867 HTTP C command argument
vectors and every existing RTTI flag remain unchanged.

## Configuration and build

`check_configure.py` extracts the exact current exception block from
`app/painter/meson.build` into isolated Meson projects. Its five cases pass:
native setup, an earlier global exception disable, the old silent unsupported
flag omission, rejection of an unsupported required flag, and rejection of a
compiler wrapper that accepts the switch but leaves exceptions disabled. The
wrapper controls use the native compiler; they do not simulate or certify a
different platform's ABI. `configure-controls.json` records the expected failures
as successful controls.

`native-build.json` records successful rebuilds of both configurations' core and
display archives and foundation executables. Separate verification manifests
redirect only 17 protected enum/PDB generator recipes to fresh isolated source
copies. Only core/display enum generation ran for these bounded targets; its
generated C bytes match the originals. Compiler and linker recipes are unchanged
from Meson output. All 9,938 tracked source-file hashes remain unchanged during
the builds. No full application relink or application execution is claimed.

`foundation-native.json` and the two foundation logs record 39/39 tests per
configuration with fatal GLib warnings. The new C-compiled caller exercises
typed Painter Error, `std::runtime_error`, `std::bad_alloc`, and non-standard
integer exceptions through result and void boundaries, with and without the
optional error output. These 16 calls verify mapped errors, cleanup on unwinding,
and return to the C caller. The shared helper's reference and definition remain
unmangled in the actual C/C++ objects.

## Reproduction

Source the existing native environment for the selected configuration before
running the checker. Generated build prerequisites must already exist. For
example, from the repository root:

```sh
source /workspace/shared/gimp-cpp-boundary-04005/env-main.sh
python3 -B tools/check_painter_exception_flags.py --self-test
python3 -B tools/check_painter_exception_flags.py \
  --metadata-dir /workspace/shared/gimp-cpp-boundary-04005/build-default \
  --output /tmp/painter-exception-default.json
python3 -B migration/tests/exception-policy/check_configure.py \
  --work-dir /tmp/painter-exception-configure-fresh \
  --output /tmp/painter-exception-configure.json
```

Use `env-http.sh` and `build-http` for HTTP command replay. Configure control
work directories must be new. Neither checker regenerates the GIMP build,
launches GIMP, reads a user profile, opens a listener, or writes production
sources. The evidence bundle preserves historical command snapshots, current
metadata, executed tools, native manifests, selected logs and SHA-256 identities;
raw Meson environment logs and isolated source copies are excluded.

These are Linux/GCC 14.2 native results. MSVC, clang-cl, macOS, other compilers,
cross builds, future translation units and future header changes require their
own verification. The standalone flag probes establish compiler policy and
ordinary C++ unwinding; the 39-case foundation run establishes the bounded common
C-boundary behavior, not every production feature callback.
