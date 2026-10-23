# Procedure configuration zero-count copy prerequisite

Declared public base: `8f7cf21b5246e8261f3675d29f9034bb3eafb5fc`. This is a separate prerequisite fix for the
sanitizer failure found while exercising the first 08.017 widget-store subset.
It does not accept that subset or close any migration duty. 08.017 and all 93
source duties remain TODO; original task counts remain 140/661.

## Narrow production correction

Guard both argument-array copies in `gimp_procedure_create_config_with_prefix()`
by their own positive counts. This also avoids arithmetic on the empty
allocation. Preserve allocation size, property order and all nonempty behavior.
Guard the downstream copy in `gimp_config_type_register()` because its existing
precondition expressly permits NULL specs with a zero count. No broader copy
sweep, ABI change or unrelated refactor is included.

## Focused executable regression

The standalone `procedure-config` Meson test uses public libgimp/config APIs,
valid local channel objects and no GIMP process or display. Its five cases cover
zero/zero, nonzero/zero, zero/nonzero, nonzero/nonzero and direct empty generic
config registration. Assertions check counts and NULL inputs, property order,
metadata, defaults, type caching, independent instances and parent association.

The native baseline passes all five, while the standard ASan/UBSan baseline
aborts at the intended empty-copy expressions in four separate cases. The
nonempty/nonempty sanitizer control passes. After the three guards, native and
ASan/UBSan both pass all five registered cases. This shows why a native-only
pass was insufficient. `baseline-red.json`, `native.json` and `asan.json`
contain portable TAP and exact diagnostic/source identities, not raw logs.

## Standard instrumentation and expanded coverage

Both expanded native and standard ASan/UBSan runs passed 205 cases each across five selected registrations.

Full standard native and sanitizer application builds completed before the
focused tests. Address and undefined instrumentation, RTTI and vptr checks are
retained; leak detection is disabled and third-party libraries are not
instrumented. `sanitizer-instrumentation.json` records the inspected build facts.
Expanded runs include the parent's pending 08.017 editor/store subset, so they
are supporting integration evidence, not a claim that an isolated four-file
prerequisite checkout was tested. No full aggregate, platform or physical-input
acceptance is claimed.

## Reproduce and provenance

In an ordinarily configured native build, compile `test-procedure-config` and
run `meson test -C build-native --no-rebuild --print-errorlogs procedure-config`.
Repeat in a standard `-Db_sanitize=address,undefined` build with
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1:abort_on_error=1 and UBSAN_OPTIONS=halt_on_error=1:abort_on_error=1:print_stacktrace=1. Use the project's normal runtime
library environment; do not disable sanitizer checks or replace production
implementations. Each baseline failure is isolated using its GLib `-p` case.

`source-hashes.json` supplies original and final SHA-256 identities. The public
base was verified by the publication owner; the local validation checkout lacks
that commit object. Before publication, verify the three original files match
those preimages and that the new test path is absent at the declared public base.
This directory contains no environment dumps, machine paths, temporary paths,
network endpoints, authentication material or raw stack traces.
