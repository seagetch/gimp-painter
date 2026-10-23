# Helper parameter name binding

Task `16.002/parameter-name-binder` passed on 2026-10-04. This is its bounded
Phase A acceptance record only.
See `acceptance.json` for results and exact source/binary identities. The next
semantic-policy, editor, storage integration and aggregate acceptance stages,
all open parent gates, and other platform/full-port work remain separate.

`app/core/gimpfilterprocedure-arguments.*` uses the core procedure's GParamSpecs
and default GimpValueArray. It validates each admitted route's existing exact
name/type/default/range/choice policy, requires the image-procedure context at
slots 0/1/2, and resolves subsequent inputs by name. Every context/input must be
assigned once. The provider, path, registration and public/hidden checks remain
in `gimpfilterprocedure.cpp`; the legacy request/wire slots are unchanged.

Values are bounded and exact-typed before any owned copy. Validation operates
on a working copy and rejects either a TRUE return or any exact typed change,
including the core-object-array class callback's empty-to-NULL change. That
callback returns FALSE, while the public GLib wrapper detects the replaced
outer pointer and returns TRUE (verified at runtime on 2.84.4; source-only
comparison also confirms this wrapper behavior in 2.70.0). A separate scoped
test-only callback changes a later numeric array element in place and returns
FALSE through the public wrapper, exercising the exact-comparison fallback. Numeric arrays compare every byte; object arrays compare every element
identity in order. Production validation uses no GParamSpec value comparator; tests separately
demonstrate the old comparators' missed element differences. The caller-retained
procedure owns borrowed immutable metadata; the existing image/layer ObjectRefs
outlive validation, execution, plug-in completion and output recovery. The
boxed drawable array only owns its container.

The linked native tests construct self-consistent synthetic core procedures and
execute their receiving marshals through `gimp_procedure_execute()`. Input
reordering therefore changes both the declaration and receiving procedure; it
does not falsify metadata while sending a changed order to an unchanged plugin.
Real four-route helper pixel and native status/progress/cancel regressions use
the unmodified production registrations separately. Small Tiles has only one
route input, so it has no nontrivial suffix permutation.

Load the restored Debian13 environment, then `tools/linux-debian13-env.sh`.
Serialize build/test operations with `/workspace/shared/gimp-painter-build.lock`.
Build `app/tests/gimp-filter-layer`, `app/gimp-painter-filter-worker`,
`app/painter-filter-procedure`, `app/painter-filter-convolution` and
`app/gimp-console-3.0` in `build-installed-filter`.

Normal helper regressions use:

```
meson test -C build-installed-filter --no-rebuild --num-processes 1 \
  --print-errorlogs painter-filter-procedure painter-filter-convolution
```

Run the linked-native selections listed in `acceptance.json` with the target's
Meson test environment and the existing immutable fixture wrapper
`migration/tests/run_filter_owner_context_fixture_test.py`. These tests require
a working GTK display. The acceptance run uses the cloud desktop's native
terminal namespace; it does not claim an exec-side DISPLAY override works.
The wrapper verifies old archive and member hashes before extraction. No old
oracle is recaptured or regenerated for this task.

Focused instrumentation uses the existing
`migration/tests/run_filter_process_sanitizers.py --build build-installed-filter
--output <private-output> --lock /workspace/shared/gimp-painter-build.lock --run`.
It instruments the new binder and native tests in addition to the affected
helper, four plugins and its declared existing boundary scope. Its separate
RTTI compatibility rebuild, private executable overlay and input seals are
preserved. Remaining GIMP/dependencies are ordinary; LeakSanitizer is off.
The normal Meson run includes the 320-Blinds corpus. The focused sanitizer
runner retains its existing smaller helper selection; its Blinds historical
pixel comparisons come from the selected native context cases. These two
coverage scopes are reported separately.

The optional GLib 2.70 API-ceiling compile records only the new translation
unit compiled against the installed headers with MIN_REQUIRED/MAX_ALLOWED set
to 2.70 and deprecated API uses treated as errors. It is not a GLib 2.70 runtime
or complete minimum-version build. No new GUI behavior, XCF format, wire
format, scheduling behavior, kernel algorithm or native precision policy is
introduced by this task.

The normal full corpus and all 16 selected native groups passed. Focused
ASan/UBSan passed nine invocations including 56 native groups, with 35
instrumented and 44 RTTI-only units and no sealed input drift. Retained
early fixture failures are documented explicitly in the receipt. The logs
include two normal and five sanitizer Broken-pipe warnings, existing
localization/test-profile diagnostics and deliberately tested batch errors.
The flush-warning root cause is not resolved by this binder task.

`logs.tar.gz` contains the exact twelve original gzip log members, packaged
without changing their bytes. The receipt records the archive hash, each
compressed member hash and each uncompressed log hash. Extract the tar archive,
then decompress the individual `.log.gz` files to inspect the raw output.
