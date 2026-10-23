# Original feature entry, one native application lifecycle

`void features_entry_point(Gimp *)` is restored at its original public include
path. ApplicationRef::initialize_features delegates directly to it. This is not
a no-op alias: the sole layer-preset factory construction and loader registration
move from gimp_data_factories_init into this entry, called at the same construction
position. Existing core data load/save/clear remain their only providers.

One small native C phase field belongs to Gimp. No second BindingStore, C++
singleton registry, duplicated factory/backend, or ApplicationHistorySlot is
introduced. Core initialize publishes the next phase after its original signal,
with an owner lease across observers. HTTP startup remains after the complete
native gimp_restore call. GUI navigation remains in app.c under the original
GUI/HTTP guards, avoiding a new GUI dependency in the core/headless provider.
HTTP opt-in, token validation, loopback binding and routing are unchanged.

Restore is admitted once; nested/early/terminal calls are rejected. An attempted
startup failure is retained without retrying on later environment changes. Partial
provider results are disposed on error. Close marks terminal state and detaches
the service pointer before callbacks, with an owner lease through disposal.
The same close helper is used by application exit and native Gimp dispose fallback.
It does not duplicate native preset save/clear or change cancelled-exit policy.

## Verification and limitations

* Native11 feature subprocesses plus all six original preset entry/ownership
  groups pass. Feature tests include original signature/factory identity, NULL,
  order gating, disabled/default startup, real opted-in loopback service/start-stop,
  failed one-time startup, injected partial provider failure, nested restore,
  close during provider return, callback reentry, last-owner release and dispose
  fallback. Provider fault injection is explicitly a link-time wrapper; normal
  and real opt-in cases call the actual provider.
* Selected14-unit ASan/UBSan/float-cast instrumentation passes the same11+6.
  C++ no-RTTI is retained; vptr/LSan and dependency instrumentation are excluded.
* All421 public C11/C++14 probes and additional-route controls pass.
* Unchanged clean UI18+perspective24, HTTP18+HTTP UI6 pass on authenticated
  1080x1920x24 Xvfb. Oncanvas remains enabled. The actual image shows native
  preferences cursor selection, not a newly claimed pointer interaction.
* The real console executable compiles/links and runs `--version`. A separate
  actual console translation-unit command with an HTTP-disabled generated config
  compiles without HTTP undefined symbols; this is NOT a full disabled-app build.
* Two native mutations (duplicate restore and stale service owner pointer across
  disposal) are rejected. The old-symbol evidence is an nm audit of the previous
  tested binary, NOT a claimed executed missing-symbol linker test.
* A preliminary disposal fixture omitted ordinary Gimp context shutdown and hit
  the native finalizer's existing stale-context warning. The excluded log is kept;
  final disposal/last-owner fixtures explicitly prepare native context teardown,
  preserving all service lifetime assertions. This is not proof of arbitrary
  application finalization without its native shutdown prerequisites.

The immutable prefs/dialog/no-op receipts are replayed without comparing them to
new headers. This successor archive/source manifest binds the current combined
implementation. `validate.py --check-source` checks both chains and current files.
No prior receipts are resealed.

## Explicit remaining owner:08.018/features-legacy-runtime

Pinned legacy `afa43fae3e920210146abed514f136fd49f671b5`:
`app/gimp-features.cpp:45–126`, `app/presets/preset-factory.cpp:181–192,240–273`,
and `app/httpd/httpd-features.cpp` define a dynamically appended singleton feature
array, mutable preset config-property registration and initialize/restore/exit
signal delegation (including lazy registration order). Current factories/config
properties are statically native-owned, repeated registration is idempotent,
and startup/close are explicit native phase boundaries. These runtime/timing and
registry differences remain open criteria; the original entry row is PARTIAL.

Legacy optional HTTP started its daemon during initialize without the current
opt-in/auth policy. That automatic-listener behavior is NOT restored; any parity
resolution must preserve the current opt-in/token/loopback security contract.
Native Gimp layout changes here; historical Interface, binary and subclass ABI
are not claimed. Parent08.018 and full migration remain open.
