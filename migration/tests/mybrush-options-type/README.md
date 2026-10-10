# Original WBS 08.007: native MyPaint options and metadata ownership

GimpPainterMybrushOptions directly derives from the current GimpPaintOptions and
implements GimpConfig. Its native registration, public class macros, struct sizes,
45 double settings, five switches and two text settings are exercised through
actual GObject property calls. Every double property's serialize flag and derived
owner are checked. The two view properties are inherited from native PaintOptions;
a nondefault LIST view and size survive serialization. Resource JSON round-trips
without changing the source model, and reset follows the existing Resource default
policy. This is not a new comparison against the legacy GObject Reset behavior.

## Missing stored brush-mode restored

The pinned legacy options implementation installs brush-mode, stores it in its
setter and returns that field from get_property. The separate rendering helper
always returns Normal. These two behaviors must not be conflated. The pre-fix
native regression failed because the migrated class had no brush-mode pspec.
missing-brush-mode-baseline.json records that actual failure and source/binary
hashes. The property is now stored in the common OptionsImpl and uses the exact
old GimpMypaintBrushMode name, enum numbers, names and nicks:

- 0: GIMP_MYPAINT_NORMAL / normal
- 1: GIMP_MYPAINT_NORMAL_AND_ERASE / normal-and-erase
- 2: GIMP_MYPAINT_LOCK_ALPHA / lock-alpha
- 3: GIMP_MYPAINT_COLORIZE / colorize

The native test imports all four symbolic values, reads their numeric values,
serializes and reloads them, duplicates and copies them and resets to zero. The
native changed-property serializer omits default Normal; a fresh destination
correctly recovers it. The first new fixture incorrectly required explicit default
text, then was corrected after inspecting the real serializer. That failed fixture
is retained separately and is not evidence of a production failure. The copy test
seeds a different destination because gimp_config_copy reports change, not success.
An unknown enum nick is rejected with GError and leaves the prior known value
unchanged. Existing profile migration keeps its own raw unsupported data contract;
this native enum does not promise to represent arbitrary unknown integers/nicks.

The compatibility field is independent of Resource JSON and settings-changed.
Current parent config copy/duplicate/reset owns its normal property lifecycle.
This restores saved option state and does not claim new rendering mode behavior.
The prior profile contract's statement that the renderer helper serialized Normal
has been corrected. Pinned enum source copies prove the actual four-value ABI.

## Five dictionary owners replaced

The original input, numeric-setting, switch, text and migration getters add a
caller ref; four also have an extra initialization ref with no teardown. The seven
loader holds and options consumers were traced in the existing dictionary audit.
Additional grouped-setting/type dispatch calls take and release those same getter
refs. Their native replacements use generated immutable metadata and enum ranges,
not retained GHashTable caches. There is no heap metadata cache reference to release
at shutdown and no transferred dictionary reference for a caller to leak. Resource
and Mapping retain their mutable values by owned value/vector storage.

The added recreation test constructs and destroys consumers eight times. Each
pass parses all 45 numeric setting names with all nine input names, applies all
five switch and two text names, copies/serializes/reloads their values, and checks
all six migration aliases including their curve scaling. Native C and C++ metadata
programs additionally validate the generated categories. Grouped GUI parity remains
outside this ownership gate; no legacy heap-cache API is kept merely to imitate
its initialization/refcount sequence. This accepts the existing dict-owner child
through explicit replacement of cache/caller ownership, rather than claiming to
have tested teardown of a nonexistent cache.

All 13 native options and nine Resource cases pass; the corpus case loads and
configures 177 pinned brushes. Both metadata programs exit zero. Reports seal 26
current source inputs and detect edits during execution. The native options suite
also retains its existing callback, history, close/copy and resource ownership
regressions. A separate read-only review found no production blocker and identified
the fixture expectations above; the reviewer did not execute these tests.

Eight source-specific obligations are accepted (two original plus six existing
child), with zero routing changes. No new child is added. Whole-file source anchors
identify the relevant type/ownership duties; they do not declare all brush engine,
grouped UI, tablet, sanitizer or cross-platform work complete.
