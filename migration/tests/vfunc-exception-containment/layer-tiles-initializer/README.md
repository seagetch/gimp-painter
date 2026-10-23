# Bounded layer-tile initializer C ABI containment

Candidate child of 38.004; original 111-callback obligation remains open.
Foundations: 05.013 and 01.005/gimp3-vfunc-contract. This fixes the modern route
for old `gimplayertileview.cpp:324` construction, not unrelated callback groups.

The causal baseline in `../initializer-causal-red` uses a C caller and actual
production archives. Typed, standard and allocation failures returned non-NULL
failed widgets; unknown exception through raw GObject construction terminated.
The outer public factory's NULL for unknown exceptions was not safe containment:
the exception had already crossed GTK C frames before reaching its outer catch.

The initializer now records failure before attempting C++ setup, builds under
BindingStore's constructing state, and activates only after the native tree and
owned connections are complete. All exception kinds are contained inside the
native callback. Failure closes partially owned resources, keeps the instance
inert, and both public factories destroy/release failed floating instances and
return NULL. A raw `g_object_new` result is an explicitly failed, safely disposable
object, not a successfully active tile view. The field extends the modern native
replacement's instance layout; historical binary ABI compatibility is not claimed.
Normal dependency rebuilding recompiles callers against the new header.

The dedicated `painter-layer-initializer` C11 driver/C++14 wrapper tests 24 cases:
4 exception kinds × early ensure / after real activation × legacy factory /
context factory / raw GObject. Test-only GNU ELF `--wrap` seams do not alter normal
application linkage or inject through a GTK C callback. The late seam throws
inside the production initializer after real activation, with the fully built
native tree owned; this tests cleanup before publication, not every arbitrary
intermediate allocation position. Observed owner/child weak references all die,
raw objects expose closed/absent stores and inert operations, one local guard
unwinds, and subsequent construction/destruction succeeds. No exception is thrown
from a noexcept destructor or a GLib allocation function.

Registered target timeout is60s. Full current layer70 uses180s; canvas107 uses60s.
All normal cases passed under authenticated1080×1920×24 Xvfb with no skipped cases.
The full canvas fixture enables on-canvas UI; screenshot evidence is maintained
by the separate portrait acceptance route. Internal failure containment does not
need a synthetic screenshot.

Sanitizer recipe: `migration/tests/build_layer_initializer_sanitizers.py` builds
private instrumented archives, reusing them for initializer/layer/canvas links.
It preserves prior layer-editor/action instrumentation and canvas/store/registration
instrumentation. The reports enumerate exact files/commands/hashes. ASan, UBSan,
float-cast-overflow and consistent C++ RTTI are bounded to listed sources; remaining
dependencies are not fully instrumented. LSan is disabled. Normal objects remain
unchanged. Do not infer whole-port completion from these results.

## Verified final checkpoint

Normal24+70+107 and corrected boundedASan24+70+107 all pass. The corrected
union instruments73 sources and rebuilds37 additional C++ units for RTTI only.
Unchanged watchdogs60/180/60s; runtime3.83/147.78/49.54s. No skips, source drift
or sanitizer error. The first union omitted a required RTTI compatibility unit
and hit UBSan invalid-vptr; its logs and recipe remain in
`excluded-instrumentation-attempt/`. The correction restores the full existing
bridge RTTI set, not a vptr suppression. This task adds no new visual UI behavior.

`validate.py --check-source` verifies all three exact suites, source/archive
identity, the fault baseline and current source hashes. Raw inherited environments
are excluded. The source snapshot explicitly includes pending07.008 supporting
test additions; it is not a claim that those additions already exist in an earlier
commit. Original111-wide exception coverage remains uncompleted.
