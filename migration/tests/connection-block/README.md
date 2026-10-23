# Native signal block and after order — original 06.018

Connection forwards G_CONNECT_AFTER to native GLib registration and tracks the
blocks acquired by that wrapper. Balanced unblock removes one such block;
unmatched unblock does not remove another caller's native block. Move carries
the nesting count with the handler, and replacement/close resets the old state.
These mechanisms were already implemented; this task adds direct acceptance.

A native RUN_LAST test signal records normal handler, default class closure and
after handler as phases 1, 2 and 3. The after handler is connected first, proving
phase semantics take precedence over registration order. Tests assert the full
trace through nested blocking, partial unblock, move while blocked, moved-from
unblock, balanced and unmatched unblock, and temporary suppression of the after
handler. A block acquired through native g_signal_handlers_block_matched remains
in effect after an unmatched wrapper unblock and a balanced wrapper block pair.
Only its matching native unblock restores the handler. Replacing a blocked
connection starts the new handler with zero owned block depth. Closing both
handlers leaves only the default phase.

Existing notify ordering and target-destruction tests remain in the same suite.
The rebuilt normal and common-component ASan/UBSan suites each pass 55 cases.
All 11 common C/C++ units are instrumented, system libraries are not. LSan remains
unavailable under the verified ptrace limitation; vptr is excluded for no-RTTI
native compilation. The initial fixture draft referred to nonexistent GLib
block-by-data convenience names; the final test uses the actual matched-handler
API. This was a compile-time fixture correction, not a production defect.

Pinned delegators.hpp hunk 01.002/000042 passes its bool after directly to
native g_signal_connect_closure and implements temporary suppression with
native handler block/unblock. The modern flags preserve that after meaning and
native nesting for balanced callers, while making unmatched wrapper unblocks
safe. The old file/blob/hunk identities are verified. Only source obligation
legacy-4d4380cccbc0b15e7d63 becomes DONE.

This is the common handler ordering/blocking contract. It does not introduce an
exception-safe scoped blocker, change arbitrary callback thread requirements,
or claim every feature signal and platform has passed its separate acceptance.
