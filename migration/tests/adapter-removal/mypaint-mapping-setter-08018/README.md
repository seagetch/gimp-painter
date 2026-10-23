# Row 57: original-name mapping replacement

Public base 8ca3bf43fe61e534df38737cb24da2f89aedcbd0. Legacy pin
 afa43fae3e920210146abed514f136fd49f671b5: app/paint/gimpmypaintoptions.cpp:538–567
and header:81–86. The three original editor calls are
app/tools/gimpmypaintbrusheditor.cpp:425,427,503. This is a complete replacement
of one setting/input curve, despite the singular function name.

The original void C signature and named handle are restored through the existing
operation-level Options alias. Name normalization duplicates the string and uses
the exact g_strcanon ASCII letter/digit/underscore allowed set, replacing every
other byte with underscore, without lowercasing. Switch/text names are invalid.
Caller buffers are borrowed only during the call and remain unchanged.

Zero points explicitly clears the input mapping, including with NULL points.
Otherwise 2–8 points are required. Finite, float-representable coordinates round
through float before native validation and publication. X ordering is checked
after rounding: duplicate X and descending doubles that collapse to equal floats
are valid. No sorting, deduplication or UI-range clamping occurs. Invalid requests
are rejected atomically without draft mutation or notification. The original
could assert, crash or partially mutate on malformed input; those behaviors are
not preserved.

The wrapper delegates once to the existing native set_curve operation. That
operation stages a complete Resource copy before publishing, sets dirty state,
and owns revision-guarded notifications. Empty inputs are retained as explicit
empty arrays. Removing the input key would incorrectly restore the default
opaque_multiply/pressure mapping. Modern native APIs and editor writes retain
full double precision; they are not rerouted through this legacy float bridge.

Row 57 remains PARTIAL: the original setter notified only its named property,
whereas the native model emits its existing broader property notifications and
one settings-changed signal unless a reentrant callback supersedes the revision.
The wrapper adds no second notification. History records the edited draft when
leaving it, not each point edit. The selected saved resource is not mutated.
The existing active value-owned draft remains writable without a selected source;
constructing, unavailable and closed models fail without creating a new draft.
This model distinction is explicit and does not satisfy row 50's borrowed live
GObject identity contract.

The original paint core split its active stroke on options notification before
copying mappings (gimpmypaintcore.cpp:260–285). The current session reacts to
settings-changed and PaintCore::configure finishes the active stroke before
applying the new resource. Existing Undo, cancellation and notification-reentry
boundaries remain owned by those native implementations.

No legacy instance/class layout, runtime GType or binary ABI compatibility is
claimed. Only row 57's implementation evidence changes. The other 75 entries,
including row 36, row 51 and partial row 58, separate 31 hooks, type/handle catalog,
overall 141/661 and task 08.018 are unchanged. ImageGenerator retirement and the
pending perspective change remain excluded.

## Final acceptance

111 native and 111 ASan+UBSan cases passed across seven selected suites.
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

The editor visual demonstration is opt-in and excluded from the aggregate:
GIMP_TEST_MAPPING_SETTER_VISUAL_DEMO=1 with test path
/painter-editor/mapping-setter/visual-demo. It shows real native curve controls
before replacement, after API-driven replacement, and after explicit reset.
See report.json for exact source, binary and evidence hashes and explicit limits.
