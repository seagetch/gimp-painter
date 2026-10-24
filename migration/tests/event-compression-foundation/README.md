# 23.014: old motion-compression ownership foundation

This is a source-only bounded test of the **active** old compressor and its
outer dispatch wrapper, pinned to `afa43fae3e920210146abed514f136fd49f671b5`.
The disabled historical alternative is not used. No old GIMP application runs.

The literal functions execute against a defined queue host. An independent
boundary-index oracle covers 4,163 cases: 0–64 matching motion events, 0–3 tail
events, optional null retrieval, release/key/crossing, modifier state, device,
same-widget different-window and different-widget boundaries. It proves that
the initial borrowed event is untouched; acquired superseded motions and the
first terminal event are freed exactly once; the terminal event dispatches after
the latest motion; later queued events remain untouched. A mock internal canvas
handler consumes the retained motion; this is not full old handler execution.

Normal and ASan/UBSan runs pass. Six causal mutants fail assertions: ignoring
state/device/window boundaries, dropping terminal dispatch, missing terminal
free, and double terminal free. LSan is disabled because this execution host
reports that LeakSanitizer cannot run under ptrace; no leak-test claim is made.

This does **not** establish current GTK/GDK delivery or device history behavior,
real pen/tablet handling, Windows behavior, full old GUI parity, or completion
of original 23.014 and its dependencies. Native GTK owns dispatch event leases;
application callbacks must not free borrowed events. Native order and ownership
need separate evidence; adding the obsolete queue to production is not implied.

Run `python3 run.py`, then `python3 validate.py`. The artifact seal describes the
recorded run; regenerate it only after reviewing intentionally changed evidence.
