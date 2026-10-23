# Native GTK ownership — original 06.023

The migrated construction paths use native GTK3 containers, explicit
ObjectRef::sink/retain/adopt and the common BindingStore/Connection lifecycle.
They do not retain a second Definer owner or a temporary Packer borrowing a
Definer. GTK parent ownership and an explicit strong reference may legitimately
coexist; a copied reference alone is not evidence of a double-unref defect.

The pinned provider is glib-cxx-def-utils.hpp, blob
b8db98b39de4e436b74b348a4a4fe2e0a4af1dbc. Its raw constructors cannot distinguish
a newly floating widget from a borrowed widget, and Packer stores Definer&.
The replacement owns new floating widgets explicitly, retains borrowed widgets
before reparenting, and closes delayed callbacks before destroying roots.
Widget destruction and GObject finalization remain distinct: retaining a
logically destroyed widget does not permit continued use of it.

## Current execution

The Linux native build passes 21 GTK cases across 12 processes; every process
exits zero under G_DEBUG=fatal-warnings. This includes ten compact-options cases,
five canvas/tile cases, three MyPaint editor cases and three layer-dialog cases.
They exercise mixed start/end packing, expand/fill/padding/order restore, three
orientation cycles, retained/detached children, popup repetition, owner closure,
callback reentry and tool-group rebuilds. Five explicit object lifetime counters
verify destroy and finalize occur exactly once at their proper stages.

The same ten compact cases pass ASan/UBSan after rebuilding six listed
common/compact/options/test translation units. Native dependencies and remaining
application archives are uninstrumented. vptr is excluded for native no-RTTI;
LSan is disabled under the previously verified ptrace limitation. This is a
focused ownership check, not a process-wide leak or all-feature sanitizer pass.
Missing-icon/theme notices remain; packaging acceptance is not inferred.

The first native run exposed an invalid painter-settings GParamSpec default:
GimpConfig reset passed NULL to a JSON setter that correctly rejects NULL. The
default is now valid version-3 empty brush JSON, equivalent to Resource(). All
11 options tests pass, including a new repeated-reset test checking exact default
encoding, unchanged saved brush data and continued C API rejection of NULL.

Isolated canvas/editor cases also skipped the first case's deferred device
initialization. Their bodies passed but GUI teardown failed with a fatal
gimp_device_info_save_tool assertion. Each case now explicitly establishes
focused-once initialization before its body. Those failing processes are retained
as failures in evidence; a TAP “ok” line alone is never counted as a passing run.

The viewport fixture initially assumed destroying a scrolled window destroys an
independently retained non-scrollable child. GTK 3.24.49 removes that child from
its automatic viewport first, leaving it unparented. The corrected test follows
[the upstream removal implementation](https://raw.githubusercontent.com/GNOME/gtk/3.24.49/gtk/gtkscrolledwindow.c),
also checking GtkScrollable children attach directly. No product change forces
different native GTK semantics.

native.json and sanitizers.json seal source and executable hashes and successful
TAP output. evidence.tar.gz contains commands, compile flags, logs, earlier
failed attempts and hash manifests. The options-reset evidence is in
options-restored-result.json within that archive. The runner requires an existing
display, uses the native Meson test environment, stops on nonzero exit, and
validates binary/log hashes before reusing an unchanged successful prefix.
The saved October 9 results were recovered after the runtime reset; source and
log hashes were revalidated, not described as freshly rerun tests.

## Source-specific coverage

caller-census.tar.gz contains all 71 byte-verified changed C++ source/header files
from the pinned source: four callers, one provider, eight include-only files and
58 other negatives. This is the changed-C++ scope, not a whole historical-tree
absence claim. native-ownership-map.json maps the provider and four callers to
the native layer tiles, shell canvas, canonical layer dialogs, compact controls
and common ownership handles. Its prepared test list is a crosswalk; execution
claims are only those in the current reports above.

The bounded routing proof retains 15 legitimate provider/caller/support duties,
removes exactly 78 unrelated native GTK layout/model TODO duties and adds three
previously missed direct callers. All other 372 duties on the reviewed original
hunks and all 535 earlier DONE rows are preserved. Routing adds zero DONE.
After the ownership acceptance, the resulting 18 exact 06.023 duties become
DONE: 22,868 source duties total, 553 DONE. Original WBS completion increases from
100 to 101 of 661. No other original WBS item is completed.

Run the bounded source/work proof and its 12 positive/negative controls with:

```
python3 -B migration/tests/gtk-binding/reproduce-gtk-routing.py --check
python3 -B migration/tests/gtk-binding/test-gtk-routing.py
python3 -B tools/audit_legacy_granularity.py --check
python3 -B tools/check_tasks.py
```

Existing file formats and consumers are unchanged. ROUTING.md describes the
exact 51-blob/96-hunk correction, write guard and retained source identities.
Complete historical source/base trees are absent, so no full historical
inventory-generator pass is claimed.

This closes the original Definer/Packer ownership condition. Exact historical
dropdown/preset behavior, tile geometry, tool-state persistence, all named GTK
handle adapters, platform/tablet coverage and independent feature duties remain
separate. Earlier foundation report digests remain historical snapshots; the
global acceptance checker still reports their source drift. The new 06.023 row
has matching native and focused-sanitizer evidence, without rewriting old seals
or claiming the whole matrix passes.
