# Bounded Video page proposal (source audit only)

## Pinned source findings
Old afa43fae plug-ins/common/video.c pattern_name[9] names: Staggered, Large staggered, Striped, Wide-striped, Long-staggered,3x3,Large3x3,Hex,Dots. pattern_str() emits each label with numeric(index) inside a brace enumeration. Defaults vvals={2,TRUE,FALSE}. All three operation parameters declared GIMP_PDB_INT32; additive/rotated blurbs contain {TRUE,FALSE}. Noninteractive copies all signed ints, with no pattern range guard.
Pinned app/widgets/gimplayerpopup.cpp lines435–543 generic INT32 path parses brace enumeration: recognized labels become integer combo; TRUE/FALSE becomes checkbutton initialized from cur_value, and toggled writes0/1 to proc_args. It connects handlers after initial set_active. This supports source-level dropdown/checkbox design, not an assertion about executing that old popup or full generic API parity. No old or held popup runtime/code edit.

## Existing specialized page
Use existing app/dialogs/painter-layer-dialog.cpp page infrastructure and painter-layer-ui harness, not new generic schema/API or popup. Current Colorify path demonstrates guarded dispatch, hydration before signal hookup, exact context/raw reuse and checked publication. Preserve it unchanged. Proposed additional 'video' route is native-only, not one of isolated schema enum's four entries.

## Model/UI contract
- Pattern dropdown nine labels, stable integer0..8 IDs; additive and rotated checkboxes. Fresh explicit selection defaults2,true,false (old declared defaults), labels describe nonlinear-U8/RGB and rotation of pattern, not image geometry.
- Hydrate only exact six G_TYPE_INT values and pattern0..8; any signed flag maps visibly to bool. Retain original three user ints. If final checkbox boolean equals loaded boolean, reuse original signed integer (including INT_MIN/MAX); only a changed boolean emits0/1. A toggle away/back thus preserves bits and is a model no-op. Pattern-only edit preserves both signed flags exactly.
- Untouched valid dialog OK must create no history, keep exact six args and raw, and not schedule execution. Cancel and closed controls no changes.
- Unknown name/type/count or invalid pattern stays Keep, with no fabricated defaults/reconstruction. Explicitly choosing Video is replacement consent and yields supported new arguments; preserve original raw as provenance. Reuse context slots only when their exact INT32 type contract is valid; never accidentally inherit invalid context types via existing generic preservation loop.
- For ordinary valid edited Video preserve all three saved context integers and raw bytes. Only requested operation fields change; Undo/Redo restores exact model/raw/context.
- Capture pattern+two booleans as fixed scalar draft inside guarded response before image/item refs; validate dropdown ID, check selected route/closed/model revision and all captured draft values again at prepublication callback. Reject rollback on callback toggle/route/revision edits, not stale commit. Creation close callback uses existing construction rollback. No string allocations added to unguarded C signal path.

## Required proof
Expected missing-choice red on committed provider; actual per-case GimpDisplay, root1080x1920 and explicit oncanvas visible/config assertions (not env-only). Cases: creation/pixels with independent pattern row oracle; untouched signed flags no-op; pattern-only edit preserves flags/context/raw and Undo/Redo; each toggle edits only own field, toggle-back no-op; unknown/malformed/invalid pattern Keep then explicit replacement; stale/reentrant draft rejection; cancel/closed/creation-close lifetime. Render raised actual controls before and patterned canvas after, programmatic GTK signals accurately captioned. Preserve Colorify7 current cases and scoped sanitizer evidence. No full generic editor, highprecision, old-runtime or gesture claim.

No production edits made for this proposal. Runtime links wait for exact aggregate release.
