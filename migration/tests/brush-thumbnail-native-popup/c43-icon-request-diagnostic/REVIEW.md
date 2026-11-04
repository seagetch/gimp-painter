# C43 requested-icon diagnostic candidate

Status: LOCAL / UNPUBLISHED. Task308 remains DOING and unaccepted. Publication requires root review and exact approval; this document is not approval.

Base C42:180446d7a1dfe92739b8bd1881922d5d384dfb26; tree a4166d0e0bc3e4983fc9cf93b914eb84d15c2e50. Required sole parent remains e25a94e21efca6e6604ddac6f15570c049f421e7 under existing task policy.

## Why this observation

Run59 (41604222306) ended5success/3failure. Original sanitizer case14 firstwait12,345,052us, dispatch12,341,193us and stable0 still fail the unchanged3s predicate. Other20 leaf assertions pass but processes report199,751–202,391B leaks. Normal Brush21 both scales and fixed mask107 pass. Matched job current is separately pinned b6718253, NOT C42 and not a C42 performance comparison.

The separate debugger profile observed89 image-width calls:78 distinct tracked GtkImages observed once,5,848,963us inclusive total;11 untracked calls2,050,610us. Tracked item-tree33calls3,392,421us and editor43calls2,456,540us dominate; tracked Dockbook2calls only2us with pixel12. Expensive tracked entries use icon-name storage and logical sizes with pixel-1. Eleven destroyed snapshot objects cannot be paired with11 untracked calls. Untracked first/last metadata does not describe every untracked call. Existing same-size guards are present. Dockbook tab recreation is plausible but cannot establish dominant cause. No production repair is justified from these observations.

Official GTK3.24.49 GtkIconHelper loads a surface to measure logical-size icon names, whereas explicit pixels change FORCE_SIZE and can move work to drawing. Therefore no pixel override, icon prewarm, test delay, sanitizer change or speculative cache is introduced. Source references: GTK gtkiconhelper.c blob e51f8dcdb9848b70fa9fed4b68b251374f8b722b lines263–264,431–475,646–663. No downstream Debian patch byte identity is asserted.

## Integration and safety

Only the existing separate diagnostic overlay is extended. New icon.inc is included after image.inc and before leaf.inc. Setup before original timer; finish before image cleanup. Existing image and original leaf protocols remain unchanged. Image wrapper order: image entry, icon entry, saved original exactly once, icon exit, image exit. Removing the new two lines recovers exact C42 leaf; removing all four profile lines recovers the fixed C41 forwarding implementation. Original class inheritance/restoration code is unchanged.

Strict numeric parser validates ordering, shape, ranges, exact dictionary ordinal constraints, unique config groups, metric conservation, overflow and flag consistency. Collector requires all owner/phase/leaf/image/icon captures, verifies image/leaf and icon/image call counts, and carries parent image coverage into icon interpretation. It retains the owned-child cleanup path and never resumes GUI after the terminal observation. Report caps remain1MiB report/2MiB wire, eight96-frame samples; combined maximum serialization is regression-tested rather than increasing limits.

Original primary source,3s predicate, stable assertions, protected26, leak and sanitizer settings, workflow and controller are unchanged. Safe source/synthetic tests do not establish native speed or leak acceptance. Full candidate tests and independent review are recorded in the accompanying proof artifact. No native GTK/GIMP, ptrace, babl or ASan/LSan execution is claimed locally.

# Requested-icon diagnostics, local C43 only

Files: icon.inc, synthetic-icon-prefix.c, synthetic-icon-suffix.c, test_icon.py.
Include icon.inc immediately after image.inc, before leaf.inc. Call begin after
image_observe_begin, finish before image_observe_finish. In direct GtkImage leaf
thunks: ImageCall enter; IconCall enter; original exactly once; IconCall exit;
ImageCall exit. No replacement thunk forwarding, restoration, timers, readiness
checks, first-wait three seconds, protected-26 or leak policy changes are made here.

This records exact requested icon-name equality within one run. It does NOT prove
resolved asset identity, cache hits/misses, coldness, theme loading, rendering,
critical-path attribution, or readiness improvement. Distinct requested names may
resolve to the same asset; equal names may resolve differently by config/theme.
IDs are run-local first-observed ordinals, never hashes or stable identifiers.
Only status1 permits requested-name equality comparison. Status2 null,3 non-icon,
4 overlength,5 dictionary full,6 invalid metadata never prove equality. Aggregating
unknown calls by config/status does not assert their requests are equal.

## Bounds and semantics

At most256 names, max127 bytes plus NUL, inspected with a128-byte bounded scan.
Comparisons <=256*127 bytes; copies <=128. Dictionary capacity still searches
existing keys. At most128 config groups, then an explicit overflow aggregate keeps
calls/exits/nesting/durations without configuration claims. Every observed call
including widgets outside the image snapshot enters one group or overflow until
1e9 call cap; skipped calls increment capped dropped counter. Tracked only means
an exact live weak slot match from IMAGE_DIAG snapshot, not unique-new-image proof.
No names, paths, addresses or hashes are printed. Internal names are cleared at
finish. Metadata/config reads happen before the original; exit uses scalar token
only and is safe after destroy/finalize. Durations are inclusive and overlap under
nesting. Metadata work changes outer IMAGE_DIAG/leaf timing; do not compare that
timing as uninstrumented performance. Flags and missing/malformed records make
coverage incomplete. Existing snapshot semantic/type/bounds flags still apply to
image metadata, and downstream interpretation must combine IMAGE_DIAG caveats.

No explicit allocation, registration, ref, signal setup, string-returning property
get, theme lookup or style-context query occurs in the hot path. GValue boolean
setup occurs once before timing; local copies hold scalar payload only. Existing
image_metadata_read is reused after source review: bounded ancestor search before
scale getter, storage/logical/pixel/state/host getters only, no loading. Snapshot
weak table is read only at entry and not modified by this module.

## Getter review

Reviewed local official GTK3.24.49 gtkimage-3.24.49.c:1502-1517 public
 gtk_image_get_icon_name returns borrowed helper pointer and optional logical size,
not g_object_get icon-name (which copies string). gtkimage.c:561-562
PROP_USE_FALLBACK sets boolean using helper; gtkiconhelper.c:794-797 helper returns
priv->use_fallback. Caches GtkImage defining GObjectClass getter and validated
GParamSpec once: exact owner GtkImage, readable, boolean, nonzero property ID.
Entry invokes that same callback with initialized boolean GValue, ONLY for exact
GtkImage runtime type. Subtypes fail closed status6 rather than bypass overrides.
This deliberately preserves boolean semantics for exact GtkImage while avoiding
variadic/property-name lookup and unknown subclass allocation/loading behavior.
No claim of Debian patched-source byte identity. Official GTK3.24.49 gtkwidget.c:12065-12073, blob
a565e58b7d3ec2d2a69c1a16c54ea798bef263fc, gtk_widget_get_direction checks the
widget and returns priv->direction or the global default. No lookup, allocation
or parent traversal. Actual GTK is NOT linked or executed by tests.

## Protocol v1

Only decimal integers; fixed spaces, one record per line, strict order:
ICON_DIAG summary 1 enabled flags names groups entries exits depth dropped
ICON_DIAG group id status name_id valid storage logical pixel scale state direction fallback ancestor host tracked entries exits depth nested total_us max_us
(repeat ascending contiguous id1..groups)
ICON_DIAG overflow entries exits depth nested total_us max_us
ICON_DIAG complete

Flags:1 setup;2 overlength;4 dictionary cap;8 config cap;16 counter/duration cap;
32 invalid clock;64 unmatched/inflight;128 invalid metadata/unsupported subclass.
Invalid status6 canonical fields: name_id0 valid0 storage0 logical-1 pixel-1
scale/state/direction/fallback/ancestor/host0; tracked is independently0 or1.
Other statuses valid1; statuses1,2,4,5 storage5; status3 storage!=5. Non-icon
fallback can be1 because property config persists. IDs not referenced by a stored
group can occur when only overflow sees a newly interned name. entries/exits/depth
sum stored groups plus overflow; dropped is separate. No record itself is eligible
for acceptance or readiness proof.

## Test command

python tools/brush-ci/readiness-owner/test_icon.py -v
(or python c43-icon-module/test_icon.py -v before integration).
Real libgobject/glib runtime; synthetic GTK signatures with exact-property and
lifecycle behavior. No GTK native GUI, ptrace, babl, network or remote writes.
Standalone test finds sibling image.inc when integrated, C42 reference otherwise.

## Report-bound review correction

Independent review rejected the initial256-config budget fixture: valid large counter records could exceed1MiB. The final128-config cap retains explicit overflow and does not alter any primary limit. A conservative serialization envelope maximizes every numeric field width, existing record capacity, nine96-frame stacks, and16KiB remaining metadata:1,027,515 bytes,21,061 bytes below the unchanged1,048,576-byte cap. This is a serialization bound, not a physically realizable capture or performance claim. The counterexample is retained in the proof artifact.

## Reduced publication candidate, November 3

This local revision excludes migration/progress.tsv, tasks.md, and the large parent Brush README from the change set. Their exact base content is retained, including the unfinished task status. The user declined the ledger upload. No splitting or alternate ledger transmission is proposed. Diagnostic implementation and tests are byte-identical to the original reviewed C43 candidate; only these publication narratives and their seal hashes differ. The original independent review is historical implementation evidence, not a review of this reduced tree. A fresh review and exact publication approval are required. This changes no acceptance criteria, source-pin coverage, native deadline, sanitizer setting or protected common file.
