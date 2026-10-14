# Original named MyPaint duplicate: exact entry 36 (08.018)

The operation is `GimpData *gimp_mypaint_brush_duplicate (GimpMypaintBrush *)`,
with an owning `MypaintBrushRef::duplicate()` delegate. This restores the exact
C spelling, one-argument signature, C linkage, name preservation and transfer-full
ownership for rebuilt modern callers. It is one operation over the existing
BrushSlot, with shared staged copy logic and no second model or store.

The compatibility header aliases GimpMypaintBrush to GimpPainterMybrush and
provides only the required cast/check spellings. MypaintBrushRef aliases the
existing owning handle. This is not binary compatibility with the historical
instance/class layout, its private `p` member, old GType name or old vfunc slots.
Separate get_type, begin_use and end_use obligations are not closed. The other
75 original C entries and 31 separate core-extension hooks retain their scope.
Broader 08.018 and the 141/661 migration count remain open.

## Original obligation and callers

Pinned legacy commit: afa43fae3e920210146abed514f136fd49f671b5.
- app/core/gimpmypaintbrush.cpp:350–359 creates an owned duplicate with the native
  source name. Private duplicate at 781–799 copies settings, mappings, switches,
  text, parent-name metadata, group and icon; live icon bytes are copied at
  756–770.
- app/paint/gimpmypaintoptions.cpp:389 duplicates selection into a working draft.
- app/tools/gimpmypaintoptions-gui.cpp:190 duplicates the named draft for Save.
  The collision deletion at 185–188 occurs before duplicate and is a separate
  overwrite workflow, not part of this operation.

Modern selection has independent Resource/name values and named Save constructs
an independently named resource. Those accepted caller replacements remain
unchanged; they need not be rewritten solely to exercise the compatibility API.
Row 50's borrowed-draft getter and the Save-overwrite difference remain open.

## Required behavior

Success returns a distinct, non-floating object with one owned reference. Native
name and parent_brush_name remain independent values. Complete JSON metadata,
settings, mappings, curves, switches, text, group and unknown data are preserved.
Live icon pixels, including edits newer than serialized PNG, are independently
copied. No source file association is inherited; source name, data, dirty state
and signals are unchanged. Source/result release order is independent.

The named operation checks type, owning thread and active binding before reading
state. Invalid, unnamed, inactive and wrong-thread sources return NULL. Snapshot
or live-icon-copy failure also returns NULL without exposing a partial result.
The handle converts a failed C return into its normal C++ Error behavior. Source
and result leases cover construction, copy, dirty notifications and reentry.

Generic gimp_data_duplicate remains a distinct native operation, clearing name
and file. Its void copy-vfunc failure limitation is unchanged. Both operations
use the same prepare_copy/apply_copy helpers. The named entry must not call the
void-error-containing generic duplicate and relabel a default object as success.

## Evidence boundaries

The resource suite retains its original eight cases and adds nine actual-C and
four executed C++ cases on the supported Linux linker configuration. The test-only
--wrap=gdk_pixbuf_copy seam returns NULL once only for a chosen source icon. It
proves C failure containment, C++ exception behavior, source invariance, exact
consumption and subsequent recovery; it is not exhaustive OOM recovery or a
process-wide allocator replacement. The ordinary application has no wrapper.

The compatibility header has an explicit reviewed additional C-header route.
Generated C11/C++14 probes include it twice without an outer linkage wrapper.
Source inventory identity and unrelated header duties are not reassigned.

This is an internal API correction. Existing user-visible selection, Save,
overwrite and native generic duplication workflows are unchanged. No fixture-only
screenshot is offered as user-interface evidence. No perspective work is included.

## Final acceptance and reproduction

93 native and 93 ASan+UBSan cases passed across seven successful processes per
mode, including all 21 resource cases. Unchanged 60/120-second timeouts, no
multiplier. Actual resource/options boundary consumers compile with cc -std=c11;
the resource handle helper compiles with C++14. LSan is disabled.

Using the established isolated native or address,undefined build:

    meson compile -C BUILD -j2 gimp-painter-mybrush gimp-painter-mybrush-factory gimp-painter-options gimp-painter-session painter-options-session-c painter-mypaint-tool painter-mypaint-editor
    meson test -C BUILD --no-rebuild --num-processes 1 --print-errorlogs gimp-painter-mybrush gimp-painter-mybrush-factory gimp-painter-options gimp-painter-session painter-options-session-c painter-mypaint-tool painter-mypaint-editor

The source gate and all 48 policy tests pass; the contract checker retains
90 type/handle contracts and 76 one-to-one C routes. The strict runtime verifier
passes both modes and rejects all 12 corrupted-evidence controls. All 371 native
C11/C++14 reviewed header probes compile; both header validation registrations
pass. Independent production-source review has no blocking finding.

See report.json for exact source/binary/evidence hashes and retained failed
build stages; source-disposition.json accounts for all 76 original entries while
crediting only row 36. No broader task checkbox or migration count is changed.
