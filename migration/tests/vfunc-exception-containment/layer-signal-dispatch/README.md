# Layer signal dispatch exception containment

Bounded successor of `38.004/all-vfunc-exception-containment`; the original
111-entry parent remains open. This package covers the shared layer signal
`dispatch` helper via the actual GTK opacity signal, not all vfunc callbacks.

## Causal baseline

Baseline `91a4a58881c2575b960f9c232a7e1bc99204c253`: typed, standard and allocation
faults return safely (three exit0 controls); unknown `int` terminates with SIGABRT.
The test-only C++ ELF wrapper calls real `g_weak_ref_get` first, releases its
returned reference, then throws in C++ directly into `WeakRef::lock` / `dispatch`.
The resolved backtrace identifies those inline frames and the opacity callback
before the GLib signal frames. No GLib C function is claimed to throw and no
exception intentionally traverses native C frames.

The one-line production change adds the missing catch-all to the already-noexcept
helper. Tests require exactly one local unwind, a warning and unchanged opacity,
then a successful retry; retained controls cannot call the closed owner.

## Verification

Normal dispatch4/layer70/canvas107 passed (1.93 /94.01 /26.90 seconds).
ASan+UBSan+float-cast-overflow dispatch4/layer70/canvas107 passed
(4.26 /150.77 /51.07 seconds), unchanged registered60 /180 /60-second budgets.
Authenticated Xvfb verified1080×1920×24; the full canvas fixture explicitly enables
oncanvas UI. No new visible feature or synthetic screenshot is claimed.

The union instruments73 units and uses37 RTTI-only compatibility units; remaining
application units/dependencies and LSan are not fully checked. All native build
source hashes are retained in sanitizer-full-build.json. The10 direct/supporting
source files are archived exactly; these include07.008 tests not yet committed
at this checkpoint. Other workers' later RTTI-unit changes do not retroactively
become tested here. No source drift occurred during the build.

Run `python3 migration/tests/vfunc-exception-containment/layer-signal-dispatch/validate.py`
(or `--check-source` for current direct-source and builder hash equality).
The validator checks exact suite/count/budget sets, no skips, all artifact hashes,
source archive members and the negative-control return codes.
Prior initializer checkpoint receipts remain unchanged.
