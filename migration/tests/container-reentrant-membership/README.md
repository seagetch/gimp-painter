# Native reentrant container membership

Bounded child **08.018/container-reentrant-membership**, supporting consuming public registries. This does not close an original feature or whole source hunk.

The unchanged native container reproduces a GListModel removal from an observer model that has not seen the outer insertion (`removed=1`, observer count=0). A separate consuming tool-registration test reproduced the subsequent false missing-chain warning. Net child count is not a class-chain witness when synchronous observers undo an outer mutation.

The fix records a per-call class-chain witness, protects call lifetimes, defers nested list-model notifications into a coherent outer replacement, and detaches old weak membership before observers can re-add it. Insert/reorder and freeze/thaw honor the same notification boundary. Deliberately broken subclasses still emit the original warning and use its fallback; a correctly chained nested call cannot conceal a missing outer chain.

13 direct cases cover symmetric add/remove, other-child mutation, weak re-add and final disposal, insert/reorder removal, frozen mutations, callback freeze/thaw and delayed thaw, ordinary insertion, and both missing-chain controls. Normal and selected four-unit ASan/UBSan runs pass on authenticated 1080×1920 Xvfb. These are model-only tests with no application configuration/window: no rendered UI or oncanvas claim. Separate consuming registration integration configures oncanvas explicitly.

Three compiled production mutations fail: restoring net-count inference, disconnecting the new weak membership after callbacks, and suppressing coherent nested replacement. Restored normal/sanitizer runs pass. `verify.py --check-source` binds the nine tested sources. Dependencies, whole application, and leak detection are not instrumented/claimed.

Excluded setup failures are preserved: initial test lacked gio declaration, used a wrong property name and warning domain, then a concurrent Undo header lacked its reviewed route. Those were corrected without bypassing the guard or weakening assertions. The baseline red is preserved with its exact initial fixture; final coverage adds symmetric cases and fixes only test setup.
