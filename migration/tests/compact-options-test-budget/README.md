# Compact-options registration budget

The original unfiltered 153-target normal aggregate returned exit 1: 152 OK,
1 TIMEOUT, zero failed/skipped registrations. The timeout is retained, not
reclassified as a pass. It occurred at the registered 60-second limit.

`rectangle_units_real_popup_events` runs six tool instances and four unit groups
per instance. Every group calls twenty-five mandatory 100ms settle intervals
through pointer, keyboard, reveal and explicit settle operations. Those groups
alone require at least 60 seconds; setup adds six more settles and four separate
popup lifecycle scenarios. The other 46 registered cases follow afterward.
A 60-second budget cannot cover this target even before machine-speed overhead.
The trace shows continued normal popup progress up to the timeout.

The complete unchanged target was first measured with an explicit per-run
multiplier of four (240 seconds). All 47 TAP cases passed, zero skipped, in
99.38 seconds. The registration now gives this target 180 seconds, leaving
approximately 80 seconds above its observed normal execution time. A second
ordinary Meson run, without a multiplier, passed 47/47 in 100.17 seconds.
No assertion, event settle interval, application timeout or numerical tolerance
was changed. This is solely the test process's outer execution budget.

The final unfiltered normal run passed all 153 targets, with zero failures,
timeouts or skips, exit 0. Compact-options passed in 104.46 seconds in that run.
The exact native source patch was byte-identical before and after the aggregate.
Command: `python3 tools/run-painter-xvfb.py meson test -C BUILD
--num-processes 1 --print-errorlogs --logbase baseline-final` under the isolated
Debian environment, with no timeout multiplier. This establishes the available
Linux registrations only; it does not close UI parity, instrumented full-app,
other-platform or frozen-release-seal gates.
Original, measured and registered summaries preserve names, counts, timings,
return codes and targeted TAP/popup evidence. Original full logs remain in the
execution workspace; their hashes are recorded without publishing inherited
environment variables.
