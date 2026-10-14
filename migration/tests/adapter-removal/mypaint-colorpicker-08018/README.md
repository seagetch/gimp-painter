# Row 58 partial operation port

Pinned public base b5fb64d54a46a6a747cf2a7cbcabda99fda8c3b2. Original legacy source afa43fae3e920210146abed514f136fd49f671b5, app/tools/gimpmypainttool.cpp:212–230, 505–549; widgets/widgets-enums.h:68–72.

Original operation configures picking destination, without activating picking. The pick_colors flag was set but never read. There are no original in-tree callers. The default foreground route and existing constrain-modifier activation stay unchanged. NONE is pick-only, not disable. All four targets delegate to inherited GimpColorTool routing. The destination context is the painting tool options, not the borrowed picker sampling options.

The original symbol and named handle operation are restored through one existing typed ToolSlot, including subclass instance initialization. Invalid destination validation is an added fail-closed guard; invalid owner/thread/state cannot mutate configuration. No store creation/revival, shadow destination or new modifier gating.

Explicit source port: replace legacy GimpColorPickMode and MODE_NONE/FOREGROUND/BACKGROUND/PALETTE with GimpColorPickTarget and TARGET_* equivalents. Modern GimpColorPickMode already means unrelated display formats. No numeric bridge, macro replacement or unchanged enum/signature/ABI/layout/GType claim. Row 58 stays PARTIAL. Row 36 and all other 74 entries remain byte-identical to the pinned public base. The separate handle/type catalog, 31 hooks, overall 141/661 and task 08.018 stay open. Perspective guard remains excluded.

No ordinary application UI caller or new background-picker control was added. Any visual evidence is explicitly API-driven native integration, using actual GIMP image/context widgets, not a claimed new user workflow.

A distinct preexisting integration defect was reproduced in default native picker activation: Painter did not register its borrowed GimpTool display before delegating picker press, although GimpTool's HALT synthesizes release using that field. Active-pick HALT therefore asserted on NULL display. The narrow adapter fix records the actual display before delegation, with no post-callback restoration or new ownership. Existing HALT/dispose cleanup clears the borrowed field. Baseline failing runtime evidence is retained separately from the setter's configuration behavior.

C enum boundary validation uses the actual std::underlying_type and a checked equal-size representation copy, before any C++ enum lvalue-to-rvalue conversion. A membership switch constructs only named valid enumerators. It does not assume enum width or signedness and does not disable enum sanitization. Actual C -1 and 1000 inputs remain tested and leave the target unchanged. The C++ handle accepts valid GimpColorPickTarget values; constructing an out-of-range unscoped enum in C++ is itself undefined and is not a supported input contract. The corresponding invalid C++ test was corrected, not used to justify a sanitizer suppression.

Representation validation assumes the application's matched C/C++ ABI and compiler options; it does not support mismatched enum representations across toolchains. See the C++ enumeration/conversion rules at https://eel.is/c++draft/dcl.enum#9 and https://eel.is/c++draft/expr.static.cast#8. This is a modern source port, not a legacy ABI bridge.

## Final acceptance

97 native and 97 ASan+UBSan cases passed across seven selected suites, including
21 tool cases (17 retained and four new). All processes and X servers exited
normally; unchanged 60/120-second limits, no timeout multiplier. The new boundary
consumer is compiled by cc as C11; the tool/handle tests use C++14. LSan disabled.
373 C11/C++14 reviewed header probes and both header validation registrations
pass. Source gate, 48 policy cases, 90-contract/76-route check, strict verifier,
and 12 corrupted-evidence negative controls pass. Independent production review
has no blocking finding within the explicitly bounded scope.

Reproduce in the established isolated native and address,undefined builds:

    meson compile -C BUILD -j2 gimp-painter-mybrush gimp-painter-mybrush-factory gimp-painter-options gimp-painter-session painter-options-session-c painter-mypaint-tool painter-mypaint-editor
    meson test -C BUILD --no-rebuild --num-processes 1 --print-errorlogs gimp-painter-mybrush gimp-painter-mybrush-factory gimp-painter-options gimp-painter-session painter-options-session-c painter-mypaint-tool painter-mypaint-editor

Visual demo is opt-in and excluded from the aggregate: set
GIMP_TEST_COLORPICKER_VISUAL_DEMO=1 and select
/painter-tool/colorpicker/visual-demo in the native tool-test executable.
The demo has five eight-second stages, with real image/context colors and
painting/Undo assertions. It does not add an ordinary application UI control.
See report.json for exact hashes, failed-stage provenance and visual limits.
