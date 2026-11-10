# Preset preferences registration presence

This successor to the native catalog closes only the missing empty-registration
behavior in `08.018/preset-legacy-gui-compatibility`. It does not close the parent
GUI/runtime migration tasks or claim the old virtual/binary ABI.

The pinned old `PresetGuiFactory::prefs_entry_point()` iterates the registry and
appends only registered entries. Its empty registry returns an ordinary zero-length
GArray. The native metadata template previously caused that getter to synthesize
one preferences entry before any successful registration.

The provider now tracks process registration presence independently of its default
metadata template. Successful registration/entry makes it present; last-owner
close ends the native catalog epoch and clears it. A retired owner's outstanding
GUI lease must not hide a newly entered owner. Empty owning snapshots remain empty
across later registration, while retained populated snapshots keep their metadata.
Borrowed GArray storage, literal pointer replacement, and epoch-backed strings
retain their existing ownership contract.

The original 26 native catalog cases remain and now assert initial/final process
emptiness. Five additional cases cover independent empty arrays/snapshot refcount,
failed registration/allocation, empty-versus-populated retained snapshots, reopening
an epoch, and constructing an owner before a stale GUI lease is released. The two
new repeated-owner cases use the already documented unit-service guardian only; real preset factory,
owner, descriptor and lease lifecycles are exercised.

Validation on the final source:

- Full GIMP build: all 3,715 initial build steps succeeded. The final fixture
  correction was rebuilt successfully with the same configuration.
- Normal native cases: catalog31 + selected GUI5 + original GUI-entry6, plus
  LayerPreset12 + LayerPreset UI18 = 72. Both public-header validation groups pass.
- Eleven-unit ASan/UBSan/float-cast-overflow instrumentation: the same catalog31,
  GUI5 and original entry6 = 42. Dependencies, vptr and leak detection are excluded.
- Private counterfactual providers: the original false initial entry and removal
  of stale-lease presence restoration both fail the expected native assertions.
  The production source and normal archive are not edited for these checks.
- Actual authenticated Xvfb screen: 1080x1920x24. The selected native Preferences
  page is visible; the test directly asserts oncanvas UI visibility. The PNG was
  inspected, not inferred from metadata alone.

The initial new repeated-owner case failed on the already documented process-global
unit singleton. Its log is retained. Only its fixture was changed to share that unit
service through the existing guardian; all real catalog/factory/owner construction,
close and enumeration assertions remain. No failed result was counted as passing.
An optional session bus was unavailable in this container; final tests ran without
it. The HTTP feature is disabled, so the separately gated feature-lifecycle target
is not claimed as tested. There is no claim of two fully initialized applications.

Run `python3 migration/tests/preset-registration-presence/verify.py --check-source`
to validate the saved evidence and narrow source identities. `run-causal-checks.py`
accepts `--build` and `--output` inside the same authenticated X/dependency environment.
Historical catalog evidence is not resealed or represented as a run of the changed
provider. No normal CI workflow changed; it remains build-only.

Text log trailing whitespace is normalized; JSON captures and compressed build logs retain their captured contents.
