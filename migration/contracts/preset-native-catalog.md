# Native preset descriptor replacement

Original scope: `08.018/features-legacy-runtime` and
`08.018/preset-legacy-gui-compatibility`; pinned old source
`afa43fae3e920210146abed514f136fd49f671b5`.
The old public `PresetFactory::register_config()` replaces the map entry keyed by
`&typeid(config)` (the pointer's static type). `entry_point()` installs the built-in
configuration; a later registration before initialize can select existing
configuration properties. Merely finding only built-in in-tree callers does not
prove that this public replacement obligation is absent.

The native source-migration API preserves this supported replacement window with
an immutable metadata descriptor and the sole existing per-Gimp native factory.
It does not recreate the old C++ virtual ABI, accidental pointer-type registry,
or a second factory/resource backend. Existing readable/writable directory-list
properties are accepted; the JSON layer-preset loader/schema remains fixed.
Preentry selection is overwritten by normal entry, as in the original startup.
Plain getters do not freeze. Actual initialization or published GUI does freeze;
late replacement is explicitly rejected rather than reproducing old split-state
or uninitialized-factory behavior. These are deliberate safety differences, not
claims of arbitrary old virtual-callback or binary compatibility.

## Ownership and publication

A process catalog contains retained immutable metadata only, no Gimp pointers,
mutable resources, or factory. Gimp standard instance-private data owns its
snapshot and typed lifecycle state. Public instance/class sizes stay unchanged.
A replacement validates and constructs before publication. Descriptor, sole
factory and process selection are swapped without intervening callbacks. Old
factory release callbacks observe the coherent pair, and recursive replacement
is rejected. Owner/generation/catalog checks reject reentrant preparation changes.
Initialized owners or active GUI construction reservations prevent replacement;
failed/empty GUI consumption releases reservations without freezing.

Other preinit owners retain their independent snapshots. When all owners close,
process selection resets to built-in defaults. The legacy C getter still returns
an ordinary shallow mutable GArray, without a clear callback: callers may replace
pointer fields with literals and free/steal the array without freeing metadata.
Only dynamic descriptors actually exposed by this borrowed getter are retained
in a deduplicated metadata-only catalog-epoch list. This list is not an owner map
or resource registry, and is cleared at last application-owner close. Its count
and descriptor-allocation bytes are explicitly accounted. Built-in strings stay
static forever; dynamic strings are borrowed until that epoch ends. Preentry
exposures join the first application epoch. No references cross the epoch end.

New callers needing independent lifetime use the separately named opaque
`GimpPresetPreferencesSnapshot`: ref/unref ownership, immutable read-only entry
view, and retained descriptor backing survive catalog replacement and owner close.
`PreferencesSnapshotRef` is its C++14 ref/copy/move facade. The old ArrayRef route
is unchanged and still has borrowed-string semantics; its legitimate production
consumer retains the application owner. Retained old factories are detached,
not silently attached to replacement GUI; their ordinary borrowed Gimp lifetime
contract is unchanged.

The first candidate incorrectly added an element clear callback to the legacy
array. The pre-existing C test overwrites a pointer with a literal and expects
static default backing to survive array free: this really is supported by the
pinned original shallow-copy implementation. Its invalid-free failure is retained
as a real regression, not waived or rewritten. The final implementation preserves
that original test unchanged and supplies ownership through the new named API.

## GUI and paths

Dialog registration, action registration, Preferences and native preset view
consume the same per-owner snapshot. Preferences retains its stable GtkStack page
key while Help uses a separately copied selected topic. The historical absent
folder icon alone maps to the existing tool-preset folder icon; another selected
icon is not overwritten. Help metadata tests do not claim an installed help page
or launch a browser. Real model load/save/reopen tests use an alternate existing
path pair and verify the selected file, not just metadata strings.

## Verification boundaries

The application unit service is a process singleton: attempting two ordinary
Gimp constructions produces `the_unit_gimp == NULL` assertion failure. That
receipt is preserved as a harness limitation, not a catalog bug. Five explicitly
identified coexistence model groups use a test-only unit-service guardian. Only
unit initialization/global teardown is wrapped; all target descriptor/factory/
Gimp construction, registration, disposal and owner accounting remain real.
Each target's unit list is cleaned, counts balance, guardian tears down last.
This proves bounded per-Gimp catalog coexistence, not two fully initialized apps.
Single-owner startup and real selected-path persistence run without the guardian.
Native fatal allocator semantics are not rewritten: new descriptor try-allocation
and candidate-preparation rejection are the bounded recoverable failure tests.

Required startup versus optional construction is explicit: `dialogs_init()` has
no recoverable result and native startup assumes its complete factory; missing
or invalid required lease is an immediate diagnostic failure, not a later NULL
factory crash. Optional action setup still installs unrelated native actions
when the preset lease is unavailable. Preferences' native dialog-factory caller
already accepts a NULL constructor result; lease allocation failure returns NULL
before autosave/config changes and does not freeze, and a retry builds the whole
dialog. It does not publish a partial Preferences window.

Array consumer audit: production typed `ApplicationRef::preset_preferences()`
adopts only the legacy GArray storage into `ArrayRef`; its caller retains the
application owner. No legitimate production consumer outlives that owner or
frees individual borrowed strings. Native Preferences uses its own owner GUI
snapshot, not the process getter's strings. The unchanged original C test verifies
literal pointer replacement and permanent built-in strings. New model tests retain
32 legacy arrays through repeated replacements, verify deduplicated backing and
zero byte/count accounting plus exactly-once tracked frees at epoch close; array
free never owns strings. New owning snapshot tests read after close and verify
last-reference release separately. No leak-sanitizer guarantee is inferred.

Zero-owner preentry semantics: registrations and borrowed-getter exposures belong
to the upcoming catalog epoch. They are retained through the first owner's entry
and released when the last eventual owner closes. A process that only performs
preentry borrowed queries and never creates an owner has not reached that teardown
boundary: its retained metadata remains accounted until process exit. This is
explicit open-epoch retention, not an assertion of zero live allocations or a
leak-sanitizer result. Utility callers needing independently releasable data should
use the owning snapshot API and avoid legacy borrowed exposures. The audited
production legacy caller is application-scoped; no existing legitimate consumer
needs dynamic borrowed strings after last-owner close.

### Preferences publication cleanup successor
The exact edit/live configuration endpoints and prior autosave state are held
across descriptor lease publication. Constructed widget data is attached before
lease release. On refusal, only that widget's two manually connected theme
observers are disconnected, its tool editor is cleared, config copies/widget
are released exactly once, and prior autosave is restored (TRUE or FALSE).
No Gimp access follows lease release. Successful dialogs retain the original
native borrowed-application lifetime contract. A causal old-provider red and
three additional native/sanitizer GUI cases cover refusal, retry, and owner
closure; prior 34-case evidence is preserved as superseded history. Relocated
default labels remain N_ extraction inputs via po/POTFILES.in.
