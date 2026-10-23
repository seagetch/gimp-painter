# Original JSON resource C entries: one native root owner

The original `gimp_json_resource_get_type(void)` and
`gimp_json_resource_new(GimpContext *, const gchar *)` signatures and original
`app/presets/gimpjsonresource.h` include path are restored. The type is distinct
from GimpLayerPreset, not an alias. Its native C GObject private state owns the
only root; the preset now inherits this base and supplies validation/deep-copy
policy. JsonResourceRef is only shared ObjectRef ownership and direct C routing.
There is no second backend, legacy Interface cast, C++ placement or singleton.

The generic nullable arbitrary JSON root property retains the original tree.
Generic duplication deliberately preserves JsonObject/JsonArray sharing, while
preset input, duplicate and save snapshots remain deep copies. The generic
saver does not replace a JSON name with the resource display name. Specialized
preset defaults, object validation, unknown members, native stream loader and
name-updating save behavior are preserved. A base setter cannot bypass the
preset's object validation or clear a populated preset with NULL. The public root reference is an owning reference,
not an immutable snapshot; callers needing an independent preset tree use
`gimp_layer_preset_dup_json`.

A setter retains its resource across subclass normalization and observers,
rejects publication after disposal, cleans failure-owned nodes, and pins the
selected root across reentry. Preset metadata is updated before public root
notification so an inner replacement wins. Disposal is terminal and repeatable.

## Actual evidence

* Normal native model tests: JSON 7/7 and existing layer presets 12/12.
* ASan/UBSan/float-cast: the same 19 groups passed with 9 explicitly selected
  owner/model/test units instrumented. Production no-RTTI ABI is retained;
  vptr and LSan are disabled, dependencies and whole application uninstrumented.
* Native subclass normalization rejection, dispose and last-reference release
  are exercised. This does not claim recoverable GLib out-of-memory handling.
* C11/C++14 header receipt: 417 probes and additional-route negative controls.
  Separate first/double facade include passed C++14, no RTTI, and Werror.
* Task-isolated source contract: 1,378 records, 873 candidate dispositions,
  16 routes and 55 rejected controls, including 9 JSON ownership mutations.
  All existing field populations, defaults, flags and semantics remain covered.
  Its unrelated sources/metadata derive from HEAD, excluding paused preview edits.

Both model suites ran through authenticated 1080x1920x24 Xvfb with the registered
60/90-second budgets; neither suite is UI interaction or image evidence. Existing
preset UI tests were **not rerun**, because the shared candidate includes paused
unrelated UI changes; clean aggregate UI verification remains open. No screenshot
is claimed for this model-only change. Native tests linked the current detached
baseline; this is not a clean task-isolated application build.

`tested-owner-sources.tar.gz` pins the 9 instrumented units and 6 owner headers.
Normal results omit Meson's broad inherited environment; stdout/stderr are kept.
Excluded setup failures are preserved: unreconfigured new target, missing header
prerequisites, and test fixture teardown corrected to the normal gimp_exit route.
The baseline absence of the original header is a source absence check, not a
fabricated behavioral expected-red. Original legacy source is pinned separately;
legacy runtime was not executed for this change.

## Explicit limits

Historical C++ Interface, old instance/class layout and old subclass binary ABI
are not restored. The original filename loader was an Interface method, not one
of these two public C entries; that loader is not asserted restored. Generic
GimpData stream saving is tested, not old filename/file-identity or all historical
notification ordering. The native root ownership/factory callback safety is a
bounded modern implementation, not full legacy runtime equivalence. Parent
08.018, the broader handle catalog and final aggregate/UI acceptance stay open.
