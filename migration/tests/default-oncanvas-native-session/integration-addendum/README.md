# Default-related canvas input oracle

The old hardcoded initial-hidden assumption fails with the new TRUE default. The test now compares initial visibility to the current configuration, explicitly hides and checks the shell-local state synchronously, then retains every original input/pass-through/opacity/repeated-cycle/lifetime assertion. It never changes the preference.

An intermediate post-hide event drain failed because native image-window reconciliation legitimately reapplies global TRUE. This failed attempt is preserved. A negative-only forced wrong initial visibility fails exact equality; it is absent from final code. Dedicated native startup asserts default TRUE without an enabling override; explicit-FALSE persistence is separately covered.

These are focused default-task controls. The final cumulative four-task acceptance is recorded separately with the compact regression gate, not claimed as a test of this intermediate commit. No ASan or parent completion claim.
