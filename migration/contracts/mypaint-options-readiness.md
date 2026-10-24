# Original Options readiness: historical successful adoption

Pinned original `afa43fae3e920210146abed514f136fd49f671b5`,
`gimpmypaintoptions.cpp:606`: `gboolean gimp_mypaint_options_is_ready
(GimpMypaintOptions*)` reports whether the Options-owned brush has ever been
successfully adopted. Initialization is false, valid adoption true, NULL leaves
previous state, and dispose does not reset the value before finalize. The
archived source-derived oracle is not an old executable/UI run. Invalid pointers
and failed legacy duplication are outside defined parity.

Supported old callers include the Options popup's container selection, history
selection, context inheritance and copy. Native construction implicitly seeds a
resource; that alone must not report an explicit adoption. Current native NULL
clearing, Resource/draft/history and borrowed mutable brush semantics are not
made equivalent by this boolean restoration.

The C getter and typed `is_ready()` read one monotone scalar in the existing sole
OptionsSlot, including its closed-but-not-finalized state. No additional model,
Resource, store or GType is introduced. A transient source/revision token excludes
only the constructor's own fallback adoption. It is consumed before observers;
nested different/same explicit selections count. A request before the parent
adoption vfunc records a pending scalar and publishes true only after adoption.

Native context's real same-resource setter emits `painter-mybrush-reaffirmed`
only for a valid current brush and an unblocked observer. Existing property and
changed notifications/revision are unchanged. The new signal means successful
selection request, including copy/inheritance, not exclusively physical UI.
A standard GObject private recursion scalar avoids public instance/class growth
and preserves Config implementation. Context is retained; a weak brush lease is
upgraded after context acquisition, which may itself dispose both brush owners.
Identity and application are rechecked. The marker spans brush release callbacks
and is cleared before final retained context release. Stale/disposed handlers
reject requests. No observer and blocked-only observer cause no lease side effects.

Evidence: five causal red assertions; final normal53 (readiness20, Options24,
mode6, public enum3), selected5-unit ASan/UBSan20, header42routes/88probes.
Portrait authenticated Xvfb1080x1920 and oncanvas preference assertion are model
validation only, not rendered UI evidence. No whole-app, vptr, leak or arbitrary
native allocation-failure guarantee. The exact2ef baseline plus explicit overlay
is sealed; held providers are unchanged. Prior mode/enum receipts remain immutable;
this successor repeats their relevant semantics on the new source. Parent08.018,
current_brush, copy props and documented GUI differences remain open.
