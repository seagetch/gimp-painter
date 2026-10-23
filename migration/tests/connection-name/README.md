# 07.008/connection-name acceptance

The pinned legacy `app/base/delegators.hpp` duplicates the signal name with
`g_strdup` in its Connection constructor and never releases or uses that copy.
The modern `app/painter/connection.hpp` stores only a weak emitter, handler ID,
and block depth. It consumes the borrowed name synchronously during native
signal lookup/registration and makes no per-Connection signal-name allocation.
The permitted remedy is removal of the unused ownership, not adding an
unnecessary duplicate/free pair. No production change was needed.

The existing `/painter/signal/transient-name-lifetime` regression immediately
overwrites and frees the caller's name before emission. Both `notify` and
`notify::value` work, with unrelated detail filtering. Six teardown orders cover
emitter-first destruction, explicit repeated close, wrapper destruction, external
disconnect, untouched expired-handle destruction, and expired blocked handles.
Sixteen reconnection rounds alternate replacing a live closure and repeated
close. Every closure data payload is destroyed exactly once, stopped callbacks
remain stopped, and the emitter finalizes once without a retained strong cycle.
The owned-lifetime and reentrancy cases additionally cover moves, self-move,
destroy-notify replacement and deletion of the wrapper.

On 2026-10-15 the current source at the recorded base commit passed the complete
standalone foundation runner: 71/71 normal and 71/71 ASan/UBSan cases. Both
reports preserve actual compiler commands, output and source hashes. Reproduce
with a compatible GObject development environment:

```
python3 tools/test_painter_foundation.py --report normal.json
python3 tools/test_painter_foundation.py --sanitize --report sanitizer.json
```

The pinned Debian 13 dependencies were extracted and all 560 archive checksums
verified by `prepare-linux-build-deps.py`; their generated env.sh supplied the
GObject headers/libraries. The normal and sanitizer builds used independent
output directories. LSan was not run; exact release counters do not establish
process-wide leak freedom. System GLib is not instrumented, and the runner's
existing no-RTTI configuration excludes vptr instrumentation.

Only source duty `legacy-5202655054684d7390da` is closed. Its pinned source blob
is retained here with hash provenance in report.json. This closes the common
signal-name ownership issue, not all feature signal routes, GUI, platform,
full application or migration acceptance. Earlier inventory statements remain
historical descriptions of the legacy defect.
