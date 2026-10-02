# Legacy Painter profile migration

Source contract: `seagetch/gimp-painter`, `afa43fae3e920210146abed514f136fd49f671b5`.

## Identification and ownership

A version number, a comment or a resource name containing “mypaint” is not
Painter provenance. The importer parses configuration forms and requires a
Painter-only `gimp-mypaint-tool` identity in a tool/active-tool/ToolInfo
position, an old `GimpMypaintOptions`/`GimpBucketFillBrushOptions` type, or
Fill ToolInfo with the old `stock-id` schema. Fill keeps its identifier in the
modern port, so its identifier alone is deliberately insufficient.
The namespaced old MyPaint option filename plus an actual Painter setting, or
the old Smudge option filename plus `use-color-blending`, is also evidence: real
old option writers often omitted the tool property. Evidence is sought in
actual toolrc, context, device and tool-option records. Standard modern MyPaint
and Smudge are not remapped merely because they exist or a version is 2.x.

The first-run installer invokes the importer before upstream migration. Only
explicitly handled families bypass upstream conversion. For an origin-ambiguous
2.x profile, originals are archived with an unproven-origin report, without
publishing remapped tool state. `menurc` retains upstream action migration,
augmented only by proven-Painter MyPaint/Smudge aliases. Its exact bytes are
archived before conversion. Painter gimprc transformations are structural, not
profile-wide text replacement. The existing upstream unsupported GTK2 theme,
tooltip and gamma-UI choices stay in the originals; precision/style enum aliases
are mapped in their known containers.

## Structured mappings

| Old representation | Current representation |
| --- | --- |
| gimp-mypaint-tool | gimp-painter-mypaint-tool |
| GimpMypaintOptions | GimpPainterMybrushOptions |
| gimp-smudge-tool / gimp-smudge, proven Painter origin only | gimp-painter-smudge-tool / gimp-painter-smudge |
| GimpSmudgeOptions, same provenance condition | GimpPainterSmudgeOptions |
| GimpBucketFillBrushOptions | GimpFillBrushOptions |
| mypaint-brush context property | painter-mybrush |
| gimp-blend-tool / GimpBlendOptions | gimp-gradient-tool / GimpGradientOptions |
| stock-id | icon-name |
| default/global-mypaint-brush | independent default/global-painter-mypaint-brush |
| old paint-mode nicknames | explicit modern legacy/Painter compositor nicknames |

The old MyPaint implementation was not a registered native paint core: genuine
context output uses `paint-info "gimp-paintbrush"` with `tool "gimp-mypaint-tool"`.
Only that sibling-qualified combination becomes `gimp-painter-mypaint`.
Ordinary Paintbrush references remain Paintbrush references.

String content in names, comments and unknown extension subtrees is not edited.
Unknown fields remain verbatim in converted input and byte-exact in originals,
even if the modern loader does not reserialize them. MyPaint generated settings
are already exposed by current Painter options. Ordinary `use-texture` and
pattern references remain intact for the separately implemented paper backend.
Old MyPaint `brush-mode` and view-only preferences are retained. This slice
does not claim rendering-control parity from those bytes: the old brush-mode
getter itself serialized Normal, while inherited brush-view preferences and
the old UI naming mismatch need a separate control audit.

## Implicit old defaults matter

The old writer omitted default Normal mode. Importing that omission as GIMP3's
new Normal changes the compositor. The importer materializes `painter-normal`
for old contexts/options without an explicit mode. The named old enum table is
used; no numeric coincidence is treated as compatibility.

Old BrushCore read native brush spacing. Proven ordinary BrushCore option and
preset records now materialize `painter-legacy-brush-geometry` before their brush
fields; ordinary modern options, Painter MyPaint and dedicated Painter Smudge
are not inferred from mode or paper settings. The persistent boolean selects
the original generated/bitmap geometry, including native angle/aspect and
multiplicative hardness, and survives normal saving and brush-option copying.
One-shot config bridges resolve missing spacing and hardness after the brush
reference has loaded. Marked ordinary geometry keeps hardness as multiplier1,
so native hardness is applied exactly once. Unmarked options retain the modern
absolute native-hardness normalization. The one-shot properties return false
when read and save resulting numeric options normally. Explicit spacing,
hardness, link and provenance fields are not replaced. Painter Smudge's own
exact old mask helper already multiplies native hardness and is not normalized;
Painter MyPaint's independent engine is not normalized either.

`save-tool-options` was false by default in the pinned old application. Missing
old option files are therefore supplied with only the implied mode/native brush
defaults derived from its toolrc. Names are restricted to safe tool identifiers;
serialized names cannot become paths outside the destination. Existing edited
destination option files are never replaced or normalized by this step.

## Devices and presets

Old `GimpDeviceInfo` derived from `GimpContext`; current DeviceInfo derives from
`GimpToolPreset`. Flat old context fields move into a deliberately empty
`GimpPainterDeviceOptions` subtype. The normal exact-options-type guard remains
for other options classes. Only this named compatibility subtype may remember
any registered tool without being corrected to another tool.

The old device mask included tool, paint-info, FG/BG, brush, dynamics, pattern
and gradient. It did not include opacity, paint mode, palette, font or MyPaint
brush. Application flags preserve that distinction; explicit extension fields
are carried when present. Selecting a migrated device must not reset numeric
tool settings which that old device never stored. Device names, axes, keys,
pressure-curve points and samples remain in their actual records. The existing
unique-name list policy is unchanged.

Tool presets now have an independent `use-painter-mypaint-brush` apply flag and
mask, separate from upstream MyPaint. Their own selected resource names and full
Painter option model round-trip through the real config reader/writer.

## Tool groups

Legacy toolrc format1 wrote order, group children, visibility and active-tool.
Its `expanded` value was a runtime-only private boolean initialized TRUE, without
a GParamSpec or writer entry. No historic saved collapse state is invented.
Current groups retain the old runtime expand/collapse behavior and now serialize
it. Copying a tool structure carries name, expanded, visible and active state.
An obsolete active tool cannot trigger a critical while copying a partial group.

Migrated toolrc uses a one-load marker version1001. Its loader preserves the
imported structure and appends newly registered tools. A normal save returns to
version1. Standard MyPaint and Smudge remain independently available. Unknown
or obsolete old tools stay recoverable in the original, rather than causing the
whole imported layout to be thrown away.

## Resource paths, retention and retries

Raw old MyPaint brushes/previews move into `painter-mypaint-brushes`; layer presets
retain their resource family. Ordinary resources keep upstream family copying.
Only data-resource search paths are recovered; no plug-in/module executable path
is activated by the Painter path adapter. Custom absolute read paths retain
priority. Old application-variable MyPaint paths become the canonical Painter
family. The new profile's writable data folder stays in its search path. Old
writable paths are archived rather than activating writes into old originals.

Each handled input is archived before publication below
`painter-migration/originals/<SHA-256>/<relative source name>` in the new profile.
A mismatching archive collision stops migration. Complete files are privately
written to temporary files and published with no-overwrite GIO semantics.
Reports identify imports, existing destinations, malformed input and skipped
symlinks/special files (including link targets). Destination symlink ancestors
are rejected; source symlinks are not traversed. Nesting, input size, token count
and traversal are bounded. The old source is never changed. Repeated importer
calls keep user edits; they do not refresh an existing destination from an older
copy. The original source remains available even if a modern save omits unknown
forms.

This is the normal first-run path plus an idempotent internal importer API. It
is not a claim of automatic recovery after an OS/process crash part-way through
creating a new profile, or a completed interactive import dialog for an already
populated modern profile.

## Verification boundaries

`legacy-profile/` contains genuine old GUI writer output, explicitly distinguished
from authored seeds and synthetic malformed/unknown-value fixtures. Its manifest
records binary, source schemas, inputs and output hashes. The legacy startup
warnings and the fact that distinct Smudge seed values were not emitted remain
visible. There is no physical tablet reconnect claim.

Parser/filesystem tests cover scoped mappings, ambiguous origin, malformed
input, bounded nesting, compositor names, exact archives, symlink/path safety,
resource search paths and preservation of edited destinations. Native tests use
registered tools, GimpConfig, real device context restoration, tool-group reload,
preset numeric defaults and the real first-run installer/custom shortcut path.
Focused sanitizers do not mean the entire application/dependency stack is
instrumented. Record actual successful native/sanitizer exits before marking
individual WBS children; the complete WBS30 gate remains separate.

## Accepted checkpoint (2026-10-02)

- Parser/filesystem: 8/8 normal and 8/8 ASan/UBSan.
- Real native GTK: 9/9 normal and 9/9 focused ASan/UBSan, exit 0.
- The first-run test reads the exact installed toolrc, contextrc, tool-options,
  devicerc and shortcutsrc through registered runtime loaders, then activates
  both custom shortcut actions and verifies their selected Painter tools.
  Blend option files are renamed to the actual Gradient loader filename;
  distinct authored offset, radial type and reverse settings are checked
  through that loader, separately from genuine old writer output.
- `profile-checkpoint.json` binds these logs, binaries, fixtures and exact
  tested source hashes; `profile-tested-sources.tar.gz` preserves the snapshot.

The native harness prints a resource-save diagnostic after the test bodies.
Its source-level cause is the test-only `gimp_config_build_data_path()` branch
in `libgimpconfig/gimpconfig-path.c`: `GIMP_TESTING_ABS_TOP_SRCDIR` yields only
`<source>/data/<family>`, whereas `gimp_config_build_writable_path()` still yields
`${gimp_dir}/<family>`. The save-dir intersection in `gimpdatafactory.c` is empty.
Production defaults include the profile directory and this importer's explicit
read-path adapter appends the new writable directory. The diagnostic is retained,
not suppressed or presented as a clean shutdown log.

Old generated angle/aspect and dynamic aspect geometry remain a renderer
follow-on. When the explicit legacy-geometry backend is enabled, its original
hardness factor must replace this checkpoint's one-shot native-hardness bridge
to avoid multiplying native hardness twice; the independent spacing bridge can
remain. Archival preservation is not a claim that unsupported fields travel with
a separately exported modern preset after its unknown fields were omitted.
