# Exact row 51: owned mapping-point query

Public base 26866d5430bcc2ff1fd35786a3977059dc0b3c1d. Legacy pin afa43fae3e920210146abed514f136fd49f671b5: app/paint/gimpmypaintoptions.cpp:571–602 and header:88–94; name normalization app/core/mypaintbrush-brushsettings.c:483–487.

Restore the original void query signature through an operation-level Options type alias and the existing owning handle. The sole C entry delegates to the native same-OptionsSlot get_curve operation; it never reads the selected resource instead of the draft. The old getter returns allocated copied GimpVector2 coordinates through float temporaries, not a borrowed live brush. Each successful nonempty call now returns an independent g_free-able array with the same float-to-double rounding. Name normalization copies the input and uses the exact original g_strcanon allowed set: ASCII A–Z, a–z, digits and underscore; every other byte becomes underscore. There is no lowercasing or approximate name matching.

Safety hardening initializes every supplied output to zero/NULL. Both output pointers are required. Invalid owner/name/input, wrong thread, unavailable/constructing/closed model, allocation failure or unrepresentable/nonfinite coordinates publish no array. No clamping, partial output or getter-side model/store creation. Resource decode/curve already rejects nonfinite and float-unrepresentable input; tests must not claim corrupt live model injection where the public writer rejects it.

The current active native model always owns a Resource draft and construction seeds a standard brush. Clearing selected source does not remove that valid unsaved draft. The query therefore reads the active draft even with selected==NULL. Legacy absence of an initialized current brush maps to unavailable/inactive model failure, not to substituting selected-resource data. This is an explicit modern-model distinction; no borrowed GObject identity or lifetime claim is made for row 50.

The modern get_curve and serialized draft retain full double precision. Querying does not alter settings, dirty/conflict/name flags, source identity, history, notifications, rendering or Undo. The handle delegates once to the original C entry and returns an owning vector.

Only row 51 implementation evidence changes. Row50 borrowed-live-draft and row 57 setter remain open. Other 75 entries, including accepted row 36 and partial row 58, separate 31 hooks, type/handle catalog, overall 141/661 and task 08.018 remain unchanged. No legacy object/class layout, runtime GType or binary ABI compatibility. Internal query only; no UI change or screenshot acceptance claim. ImageGenerator retirement and pending perspective remain excluded.

## Final acceptance

101 native and 101 ASan+UBSan cases passed across seven selected suites. Options
cases increased from 18 to 20; actual C boundary cases increased from 6 to 8.
All prior registrations and assertions remain. Every process and X server exited
normally under unchanged 60/120-second limits, with no timeout multiplier.
Actual C consumers use C11 and owning-handle tests use C++14. LSan is disabled.
375 reviewed C11/C++14 header probes and both header validation registrations
pass. The source gate, 48 policy cases, 90-contract/76-route check, strict
verifier and 12 corrupted-evidence negative controls pass. Independent production
and test-source review found no blocking issue within this bounded scope.

Reproduce in the established isolated native and address,undefined builds:

    meson compile -C BUILD -j2 gimp-painter-mybrush gimp-painter-mybrush-factory gimp-painter-options gimp-painter-session painter-options-session-c painter-mypaint-tool painter-mypaint-editor
    meson test -C BUILD --no-rebuild --num-processes 1 --print-errorlogs gimp-painter-mybrush gimp-painter-mybrush-factory gimp-painter-options gimp-painter-session painter-options-session-c painter-mypaint-tool painter-mypaint-editor

See report.json for exact source, binary and evidence hashes and explicit limits.
