# Public tool button-press owner lease

`gimp_tool_button_press()` retains the tool across the subclass callback and its
subsequent control/coordinate bookkeeping, then releases that lease. A subclass
may release the caller's final reference synchronously. The lease prevents
accessing freed tool/control state after the subclass's own local lease expires.
The existing input flags and event ordering are unchanged.

The isolated native headless `gimp-tool-owner` case constructs a real tool
subclass and display, activates its control, releases the caller reference from
button_press and verifies finalization after the public call returns. It passes
normally and under ASan/UBSan with three instrumented C units: the public tool
wrapper, GimpObject and the test. Private thin archives leave production objects
unchanged. LSan is disabled; other application/dependency units are uninstrumented.
This proves the ownership boundary, not GUI/tablet event delivery.
