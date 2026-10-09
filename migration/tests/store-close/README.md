# BindingStore close transitions — original 06.011

The existing close_unchecked marks the store closing and increments its
generation before invoking hooks. Reentry in closing or closed state returns
without repeating those operations. After hooks it marks closed. Public close
holds the native owner through synchronous hooks; finalization bypasses that
lease to avoid retaining a dying owner. Production implementation is unchanged.

`/painter/store/close-transitions` exercises close from both constructing and
active states. Inside a close hook it observes closing, exactly one generation
increment and expiration of the previous generation. Calling close twice again
inside that hook preserves those observations and invokes the hook only once.
`with()` and activation are rejected while closing. After the outer
call, repeated close preserves closed state and generation, rejects generation
acceptance, and keeps explicit const reads available. Both implementations stay
alive until final native owner release. That release destroys each once without
repeating close hooks. The native owner returns to exactly one reference after
all explicit close calls.

The same suite retains the separate finalizing-close/read regression, which
checks that a finalization hook can call close again while reads reject the
dying owner. Constructor-close, in-call close and owner-loss regressions also
continue to pass. These are shared state/lifetime checks, not proof of all
feature callbacks or their execution duration.

The rebuilt native foundation and rebuilt common-component ASan/UBSan suites
each pass 50 cases, with empty sanitizer stderr. All 11 component C/C++ units
are instrumented; system GLib is not. LSan remains unavailable under the verified
ptrace limitation and vptr is excluded for native no-RTTI compilation. Explicit
reference/hook/destruction counters are not an LSan result.

Work item `legacy-fd3a25d28924dd5f4234` maps old glib-cxx-impl.hpp hunk
`01.002/000045`. The old NewGClass owns native instance-private Impl state and
explicitly destroys it in instance_finalize; it has no separate reusable close
state machine. The new shared store separates logical close from final memory
release so reentrant native callbacks cannot reopen or repeat shutdown. This
closes the common state-transition criterion, not every old adapter or parent
vfunc ordering obligation. Exact old source, Git blob, whole-file and addition-
hunk hashes are verified. Only this row's mutable execution fields become DONE;
owner-thread, feature-hook nonblocking behavior, hierarchy ordering and other
caller gates remain separately tracked.
