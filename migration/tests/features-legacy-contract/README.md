# Feature registry and supported native startup contract

This is a successor audit for `08.018/features-legacy-runtime`, not an executed old application or a blanket historical ABI/parity claim. The task's original acceptance remains unchanged. `native-replacement-map.json` traces ten explicit registration/configuration/lifecycle/GUI/HTTP boundaries.

## Corrected interpretation of the pinned source

The old `features_entry_point` has exactly one application callsite, before initialization. `factory[]` is a compile-time console/GUI preset singleton plus optional HTTP singleton array. Its internal GArray appends these results; it is not a public arbitrary plug-in registration API. Repeated entry would duplicate singleton hooks rather than create independent per-application ownership.

The old `PresetFactory::register_config` key is **`&typeid(config)`**, where `config` has static type `PresetConfig*`. It is not `typeid(*config)`. Both actual callsites choose the built-in layer configuration for console or GUI. The exact extracted method-body probe verifies static-pointer-key replacement even with two distinct dynamic classes. Its registry insertion is a small `std::map` stand-in: this establishes C++ expression semantics, **not** old GLib registry/application runtime behavior. Normal and ASan/UBSan runs pass; old source files and their pinned hashes are preserved.

Consequently no duplicate singleton registry or invented dynamic-type extension mechanism is added to the port. The sole native factory, static config properties, normal persistence route, native dialogs and explicit feature phases replace the supported startup path. Historical lazy signal timing is mapped explicitly rather than equated to current timing. Old preset data loaded in lazy restore; native data loads before the native restore signal. The native factory exists earlier, at construction. HTTP remains opt-in/token-protected/loopback and starts after complete restore; automatic unauthenticated listening is not restored.

## New native causal observations

Two new subprocess cases extend the existing feature suite from11 to13:

* Before **any** Gimp construction or feature entry in that process, the public preferences entry returns one independent descriptor. This deliberately records the difference from the old initially empty registry; it does not conceal it behind an already initialized application.
* Emission hooks observe the real native initialize→restore→exit sequence of `gimp_init_for_testing`/`gimp_exit`. The same sole factory and typed lookup are present at each stage. Startup remains rejected inside initialize and restore emission, and the provider is invoked once only after complete restore. Close does not re-invoke it. This is native model lifecycle, not a GUI startup or arbitrary old observer-order proof.

All13 feature and6 preset entry tests pass normally and with the existing selected14-unit sanitizer builder. Native GUI navigation and exact restored public metadata remain separately demonstrated by `../preset-legacy-metadata/` (24 normal/6 selected sanitizer and a real oncanvas portrait image). This audit adds no new rendered UI claim. Authenticated Xvfb is1080×1920, configuration oncanvas remains enabled. Sanitizer coverage excludes dependency/full-application/vptr/LSan instrumentation.

The clean source checkpoint is81bc24545d with committed preset metadata aa12595aa6, committed pop guard1436f90961 and Clone policy7f0333f86b; only the new feature fixture is added for this run. Held popup/canvas/preview providers equal81bc exactly and are not retried. Selected inputs are captured before build. This is checkpoint-scoped proof, not latest whole HEAD.

## Still explicit, not waived

The original child remains DOING: exact arbitrary custom-config replacement, duplicate entry/hook behavior and historical lazy observer timing are not claimed implemented. Any final closure must interpret the written acceptance against the supported replacement mapping; it must not silently convert these differences to exact parity. The GUI compatibility child retains its original criteria: public metadata is restored, but a claimed arbitrary mutable registry enumeration ABI would remain a separate unfulfilled behavior. No new extensibility is invented merely to mimic the pointer-type-key defect. Existing native persistence/accepted-versus-canceled GUI exit evidence is separate; the extracted probe does not prove file save/load or dialog restoration.
