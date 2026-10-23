# Final options publication after temporary ToolInfo lease release

Separate follow-up to committed0976ae344b and task
08.018/mypaint-options-construction; the nine-case and twelve-case receipts in
migration/tests/adapter-removal remain immutable. Original parent criterion:
`C 入口から旧 Interface::cast を排除`. This task closes a concrete lifetime gap
in the newly restored original constructor without changing native ownership.

An actual GObject last-reference toggle callback on the temporary ToolInfo lease
finds the newly constructed options through app->context_list and runs dispose
on that options object. Application binding remains active. The committed
constructor checked options before releasing ToolInfo, then only checked app
state afterward: it incorrectly returned a disposed options object. The archived
red has all original nine cases and the preceding three reentry cases passing;
only the fourth case fails `'options' should be nullptr`.

The bounded fix rechecks options BindingStore active state and context->gimp
identity after releasing ToolInfo, alongside the existing application checks.
Returning NULL destroys the unpublished owned options; its weak pointer clears.
No new store, persistent owner, GType or global lifetime redesign is introduced.
Gimp remains borrowed exactly as documented; callers must maintain native
application lifetime during options use. Readiness/GUI rows remain open.

Final source passes normal9+4 and selected five-unit ASan/UBSan9+4. The three
original fixtures/C consumer and new callback cover transfer-full ownership,
no tool-info assignment, original Mypaint name, active native model, allocation
failure, endpoint acquisition, application closure and new-options disposal.
No sanitizer coverage is claimed for all dependencies/application; vptr and
LSan remain disabled consistently with production no-RTTI ABI.

Authenticated Xvfb1080x1920x24 is verified. Real initialized-app cases explicitly
set/read back painter-canvas-ui TRUE; minimal acquisition endpoints do not render
UI. No screenshot claim. All eight providers for four paused paths match81bc24545d
before compilation, and recorded input hashes remain unchanged after testing.
This is a sealed selected-source overlay proof, not an exact whole-HEAD build.
Other Filter/Undo/fixture overlays remain ancillary and are not tested here.
