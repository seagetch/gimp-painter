# Idle source ownership — original 06.019

Source already owns the native GSource reference and its callable payload.
Close detaches the wrapper pointer before destroying/unrefing the source. The
native destroy notifier releases the callable. Dispatch retains shared state for
an invocation already underway, so self-close cannot free its executing function
or owned captures. No production API change was needed for this acceptance.

The close condition means that close prevents subsequent dispatch to the
canceled source's user data. An invocation already underway retains its callable
and owned captures until it returns. Close does not interrupt C++ control flow or
promise that arbitrary external pointers captured by a caller remain valid.
This is the same in-flight ownership principle as original06.012.

The native tests use private GMainContexts and both idle and zero-millisecond
timeout factories, requiring no sleep. They separately prove immediate explicit
close, destructor cancellation, continuing dispatch followed by natural finish,
and replacement/move/self-move. Weak capture references and destruction counters
are checked while the relevant Source wrapper still exists, distinguishing
close or natural removal from later wrapper destruction. Canceled/displaced
callbacks do not run; moved-from close cannot cancel the surviving source.
Captured objects are released exactly once.

Self-close is exercised for both factories while the callback deliberately
returns true. The source remains inactive, its capture stays alive through that
invocation, and no subsequent iteration calls it again. The existing exception
case now checks owned capture release and no later dispatch as well. Existing
reentrant capture destruction during move assignment remains a regression.

The rebuilt normal and common-component ASan/UBSan suites each pass 56 cases.
All 11 common C/C++ translation units are instrumented, system libraries are not.
LeakSanitizer remains unavailable under the verified executor ptrace limitation;
vptr is excluded for the native no-RTTI build. Explicit weak/counter assertions
are not a LeakSanitizer result. Cross-thread wrapper mutation, an interruption
barrier, timer accuracy and every feature's asynchronous acceptance are outside
this common ownership gate.

The assigned pinned delegators.hpp hunk 01.002/000042 provides generic callable
and closure-payload ownership but contains no idle-source owner of its own.
The source-specific role here is replacement of unmanaged callback payload
lifetime by the shared source adapter; the original header is not falsely
reported as having an idle implementation. Its exact Git blob, whole-file and
addition-hunk identities are verified. Only legacy-0b48a4f82224e765cc6e becomes
DONE. Other source/helper obligations and feature/platform gates remain open.
