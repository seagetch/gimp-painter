# Original WBS 06.026: inherited property delegation

The original condition is that an ID not handled by a derived property callback
reaches the correct parent handler. The existing common C hierarchy fixture
already implements that chain. This task adds direct callback tests, separately
measures GObject's ordinary property-owner dispatch, and closes the one assigned
legacy source duty. No production dispatch defect or behavioral change is claimed.

## Two distinct dispatch paths

`native-property-owner-dispatch` uses real Base and Child GTypes with separate
BaseSlot and ChildSlot implementations. It checks the GParamSpec owner types and
uses ordinary g_object_set/get. The base property reaches only the base callback;
the child property reaches only the child callback. Each changes its own value
and leaves the other implementation unchanged. This native owner-class routing
does not by itself prove the derived callback's fallback branch.

`inherited-property-delegation` therefore calls the actual child C class vfuncs
directly. A base property ID reaches child and then its fixed immediate parent
exactly once, with the same value and GParamSpec. A child ID is handled locally.
An unknown ID reaches the parent once and emits its expected invalid-property
warning, without recursion, value mutation or additional implementations.

The trace counters are test-only. Production dispatch logic is unchanged.
The existing class-local ID and owner-class rules in
[cpp-vfunc-order.md](../../contracts/cpp-vfunc-order.md) still apply: this is not
a rule to forward every invalid ID blindly in every native adapter. An adapter
replacing an existing same-class handler must preserve that specific handler.

## Exact legacy duty

`legacy-9a4db7e281551bade8cb`, hunk `01.002/000045`, identifies
`app/base/glib-cxx-impl.hpp`. The complete source copy under `legacy-source/`
reproduces Git blob `a2ad35e37a3e7a481698c1f8b78b71adb385f7d3`.
Its old GClassWrapper set/get callbacks handle their own range and otherwise
delegate to the saved previous callback. That previous callback is captured at
registration and can be a same-class native callback, not necessarily a parent.
`source-mapping.json` records this distinction and the modern explicit C vfunc
mapping. Only this duty's status/evidence fields change; its source contract and
all other ledger rows remain intact.

## Reproduction and limits

```sh
python3 tools/test_painter_foundation.py --build-dir /tmp/painter-properties --report /tmp/painter-properties.json
python3 tools/test_painter_foundation.py --sanitize --build-dir /tmp/painter-properties-asan --report /tmp/painter-properties-asan.json
```

`native.json` and `sanitizers.json` each record 63 passing cases with source seals,
C11/C++14 compilation, C header checks and six borrow-escape compile rejections.
`meson.json` separately records the incremental C/C++ build and 63 passing cases
in the native Meson target. ASan/UBSan cover the common component and actual
GObject fixture; LSan is disabled. Whole-application, platform and hierarchy
teardown acceptance remain separate. Old reports keep their historical hashes;
`validation.json` distinguishes the current task from those stale snapshots.
