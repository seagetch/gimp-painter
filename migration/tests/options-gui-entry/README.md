# Original MyPaint GUI entry forwarding

Bounded two-entry restoration, not full legacy GUI parity. See
`migration/contracts/mypaint-options-gui-entry.md` for the exact signatures,
original operation mapping and still-open semantic obligations.

The original C and typed delegates now use the sole native editor and the exact
supplied Options, including independent constructor outputs with NULL tool-info.
Normal 10 cases cover two presentations, numeric/curve peer synchronization,
canonical isolation, teardown and causal closure during acquisition/construction.
Selected 5-unit ASan/UBSan/float-cast-overflow runs 8 non-screenshot cases; this is
not whole-app sanitizer coverage (no vptr/RTTI or LSan, native dependencies not
instrumented). Both runs use authenticated Xvfb1080x1920 and assert oncanvas
preference enabled. PNGs are actual root-window captures of the native editor,
GTK-control API driven, not physical mouse interaction or visible-canvas proof.
Horizontal means full native horizontal editor, not legacy compact toolbar.

The red archive compiles C/typed consumers against committed editor and fails
at link for the missing original symbols. Its initial fixture predates the
startup correction: GUI application activation must run before tool lookup.
The first green setup attempt failed on missing tool-info before exercising the
GUI; corrected final green uses the standard GUI application lifecycle. That
setup failure remains under /workspace/shared/options-gui-entry/fixture-startup-failure.
No legacy baseline runtime parity is inferred from this link-only red.

All eight held providers and four Clone providers are byte-equal to committed
a84cd0b051e7b648ec9519ede82ae9541c44ea1c. This exact baseline includes the already
committed public tool registration change, unlike older81bc seals. None of the
four paused candidates is copied or exercised as candidate work. Filter test
fixtures are ancillary overlays; root aggregate15/17 was not a PASS. Each selected
input and sanitizer dependency is sealed, not a whole-HEAD assertion.

Run `python3 migration/tests/options-gui-entry/verify.py` to verify immutable
receipts; add `--check-owned-source` only for this exact candidate/committed state.
The runner serializes the shared build and only overlays named GUI files.
Publication remains paused pending artifact approval.

## Temporary model lease publication audit

The sealed native `gimp_painter_mybrush_editor_set_context()` obtains both an
owned `ref_for_context()` result and an owned context reference, then transfers
them into `EditorImpl.options` and `EditorImpl.context` before building controls
(editor.cpp:710–719). For direct Options input both are the exact supplied model.
The wrapper still owns the widget and checks its live binding before publishing.
Therefore releasing its temporary `model_owner` cannot be the last non-toggle
model reference in this live-editor invariant. This differs from constructor13,
whose temporary ToolInfo lease had no corresponding persistent editor owners.
No new causal last-model-release test is claimed by the existing10/8 receipts.
Application lifetime remains borrowed; no arbitrary post-return application
retirement guarantee is introduced.
