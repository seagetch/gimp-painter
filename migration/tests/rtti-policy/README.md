# Original 04.011: bounded RTTI independence

The checker inventories the complete actual production app C++ command scope
from Meson's compile database and target metadata. Fixture executables are
excluded even when they compile the same production source. It compares the
core/display source list against Meson's declared target sources, then replays
every production core/display C++ command with a final `-fno-rtti`. These are
full object compilations of the real sources, with their existing include paths,
optimization, language, exception and feature flags. Objects and compiler logs
are written only to a new, separate work directory. No production build flags
or original objects are changed; Ninja and source-writing generators are not
invoked by this checker.

Other production Painter commands are completely inventoried and must already
carry a final effective RTTI-disable flag; they are not freshly rebuilt by this
checker. Upstream `.cc` sources are distinguished from Painter `.cpp` sources.
This check does not prescribe disabling RTTI on every upstream target.

## Controls and common bridge

The small standalone controls use the real foundation target's compiler flags:

- A polymorphic `dynamic_cast` downcast and polymorphic `typeid` must each fail
  with a final `-fno-rtti`, with diagnostics identifying the RTTI restriction.
  The same isolated fixtures must compile and run with a final `-frtti`
- Ordinary virtual dispatch and an unambiguous `dynamic_cast` upcast to a virtual
  base must compile and run with RTTI disabled and explicitly enabled. An upcast
  spelling alone is not proof that legacy code needs runtime RTTI
- `typed-bridge.cpp` compiles with RTTI disabled and links the actual common
  archive. It checks GType rejection of a valid but wrong-type GObject, a typed
  BindingStore slot, virtual close dispatch and idempotent public C close calls

GType ancestry, compile-time slot identities and C++ virtual dispatch do not
require C++ RTTI. Exception matching can still emit compiler typeinfo with
`-fno-rtti`; the checker intentionally does not reject every typeinfo symbol.
The separate focused sanitizer RTTI/vptr closure remains unchanged.

## Recorded native result

`default.json` and `http.json` pass against newly restored native Linux/GCC 14.2
builds. Their complete inventories contain 73/78 production C++ commands,
including 64/69 Painter `.cpp` commands. Of those Painter commands, 48/53 already
carry `-fno-rtti`; all remaining 16 per configuration belong to core/display.
Every one of those 16 Painter units plus all four existing upstream core `.cc`
units compiled successfully into isolated objects with a final `-fno-rtti`:
20/20 per configuration, 40 total. Meson's declared core/display source set
matches the selected compile commands exactly. Original objects, production
sources and metadata retain their before-run hashes.

The original build directory disappeared during preparation. The replacement
builds use the same pinned dependency versions, but their changed paths and
binaries prevent honest reuse of the previous 04.010 runtime identities. Each
new foundation binary therefore ran once and passed all 39 cases. This includes
typed handles, typed slots, GObject lifecycle, the C bridge and exception
mapping with RTTI disabled in every common C++ compile command. No runtime case
from the lost native build is counted as newly executed evidence here.

Both configurations pass all five focused control categories described above.
`default-initial.json` and `http-initial.json` intentionally remain `FAIL`:
the first typed-bridge fixture incorrectly specialized `TypeTraits` for
`GInitiallyUnowned`, which GLib typedefs to `GObject`. The exact old source is
preserved in `initial/typed-bridge.cpp.txt`. The corrected source uses a distinct
opaque test owner type while still checking `G_TYPE_INITIALLY_UNOWNED` ancestry.

`recheck_typed_bridge.py` reran only that corrected control's compile, real
archive link and runtime. It first verified unchanged metadata/checker,
production/common sources, foundation binary/log, common archive, original
objects, all 40 successful replay objects and the passing language fixtures.
The final reports retain the initial reports' hashes and an explicit
`fixture_correction` receipt. The foundation cases and production compilation
were not repeated or relabeled as new executions. The evidence archive retains
the initial failure diagnostics and each corrected compile/link/runtime log.

## Native reproduction

First source the selected native build's environment. Existing generated build
prerequisites, the common archive and the foundation executable are required.
For a newly rebuilt native configuration, run from the checkout root:

```sh
python3 -B tools/check_painter_rtti_policy.py --self-test
python3 -B tools/check_painter_rtti_policy.py \
  --metadata-dir /absolute/path/to/build-default \
  --configuration default --run-foundation \
  --work-dir /absolute/path/to/new-rtti-default-work \
  --output /absolute/path/to/default.json --jobs 2
```

Use the HTTP environment, build and `--configuration http` for the other
configuration. The work directory must not exist and must be separate from the
native build directory. `--run-foundation` runs the real 39-case foundation once
with fatal GLib warnings. It records that this is fresh execution, with current
source/command/binary identities; it never attributes this run to an old binary.
The output JSON remains `RUNNING` if execution stops before final verification.
Only a final `PASS` establishes all checks in that report.

The optional `--prior-evidence-dir /absolute/path/to/extracted-evidence` replaces
`--run-foundation`. It requires an extraction of the published
`migration/tests/exception-policy/evidence.tar.gz` with this exact layout:

- `native/source-hashes-before-build.json`
- `current-inputs/{default,http}/compile_commands.json`
- `repository/migration/tests/exception-policy/foundation-native.json`
- `repository/migration/tests/exception-policy/{default,http}-foundation.log`

Reuse is permitted only when the complete current compile database, all recorded
common source files, foundation executable, recorded C/C++ caller objects and
runtime log match the prior SHA-256 identities, and every common C++ command
still disables RTTI. Changed paths or rebuilt binaries do not qualify. This
mode avoids repeating an already established exact-source runtime result.

## Limits

These checks are scoped to the recorded Linux/GNU-style native build commands.
They are not a whole-application link, sanitizer run, legacy adapter runtime
acceptance, GUI/profile/network-listener test, every-feature behavior test or
cross-platform acceptance. Transitional legacy adapters require a separate
source audit; the presence of old casts does not establish their safety.
