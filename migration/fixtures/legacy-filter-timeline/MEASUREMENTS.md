# Measured GUI timeline summary

Elapsed microseconds relative to run-enter. Core IDs and observed ordinals identify events within one process. These are instrumented samples, not performance guarantees. Full nesting and repeat measurements remain in timeline.json.

## async

- top, core ID 3, ordinal 1: run-enter +0us (attached=1); run-return-running +2245us (attached=1); shadow-merge-applied +125658us (attached=1); progress-end-callback +128113us (attached=1); propagation-begin +128121us (attached=1); propagation-end +128131us (attached=1)
- top, core ID 3, ordinal 2: run-enter +0us (attached=1); run-return-running +6671us (attached=1); shadow-merge-applied +85049us (attached=1); progress-end-callback +87654us (attached=1); propagation-begin +87665us (attached=1); propagation-end +87678us (attached=1)
- Final states: [{'time_us': 9432046311, 'name': 'top', 'pending': '0', 'active': '0', 'attached': '1', 'ordinal': '2'}]

## stack

- top, core ID 3, ordinal 1: run-enter +0us (attached=1); run-return-running +1947us (attached=1); stop-request-running +2015us (attached=1); cancel-signal +2016us (attached=1); stop-request-running +2026us (attached=1); cancel-signal +2027us (attached=1); stop-request-running +2041us (attached=1); cancel-signal +2042us (attached=1); stop-request-running +2045us (attached=1); cancel-signal +2046us (attached=1); stop-request-running +2064us (attached=1); cancel-signal +2065us (attached=1); stop-request-running +2072us (attached=1); cancel-signal +2073us (attached=1); stop-request-running +2074us (attached=1); cancel-signal +2075us (attached=1); shadow-merge-applied +103637us (attached=1); progress-end-callback +104527us (attached=1); propagation-begin +104530us (attached=1); propagation-end +104539us (attached=1); stack-unblocked-requeue +131000us (attached=1)
- bottom, core ID 4, ordinal 1: run-enter +0us (attached=1); run-return-running +2453us (attached=1); shadow-merge-applied +69733us (attached=1); progress-end-callback +72208us (attached=1); propagation-begin +72216us (attached=1); propagation-end +72240us (attached=1)
- top, core ID 3, ordinal 2: run-enter +0us (attached=1); run-return-running +1989us (attached=1); shadow-merge-applied +80323us (attached=1); progress-end-callback +82883us (attached=1); propagation-begin +82895us (attached=1); propagation-end +82935us (attached=1); stack-unblocked-requeue +1459987us (attached=1)
- bottom, core ID 4, ordinal 2: run-enter +0us (attached=1); run-return-running +2409us (attached=1); shadow-merge-applied +82352us (attached=1); progress-end-callback +84867us (attached=1); propagation-begin +84876us (attached=1); propagation-end +84907us (attached=1)
- top, core ID 3, ordinal 3: run-enter +0us (attached=1); run-return-running +2172us (attached=1); shadow-merge-applied +80759us (attached=1); progress-end-callback +83367us (attached=1); propagation-begin +83373us (attached=1); propagation-end +83387us (attached=1)
- Final states: [{'time_us': 9436940362, 'name': 'top', 'pending': '0', 'active': '0', 'attached': '1', 'ordinal': '3'}, {'time_us': 9436940366, 'name': 'bottom', 'pending': '0', 'active': '0', 'attached': '1', 'ordinal': '2'}]

## cancel

- top, core ID 3, ordinal 1: run-enter +0us (attached=1); run-return-running +2361us (attached=1); explicit-cancel-request +2395us (attached=1); shadow-merge-applied +134260us (attached=1); progress-end-callback +136767us (attached=1); propagation-begin +136774us (attached=1); propagation-end +136784us (attached=1)
- top, core ID 3, ordinal 2: run-enter +0us (attached=1); run-return-running +2338us (attached=1); shadow-merge-applied +80072us (attached=1); progress-end-callback +82602us (attached=1); propagation-begin +82610us (attached=1); propagation-end +82622us (attached=1)
- Final states: [{'time_us': 9441891014, 'name': 'top', 'pending': '0', 'active': '0', 'attached': '1', 'ordinal': '2'}]

## latecancel

- top, core ID 3, ordinal 1: run-enter +0us (attached=1); run-return-running +2504us (attached=1); late-cancel-request +72396us (attached=1); progress-end-callback +82562us (attached=1); propagation-begin +82573us (attached=1); propagation-end +82596us (attached=1)
- top, core ID 3, ordinal 2: run-enter +0us (attached=1); run-return-running +7627us (attached=1); shadow-merge-applied +87231us (attached=1); progress-end-callback +89785us (attached=1); propagation-begin +89793us (attached=1); propagation-end +89805us (attached=1)
- Final states: [{'time_us': 9446833253, 'name': 'top', 'pending': '0', 'active': '0', 'attached': '1', 'ordinal': '2'}]

## detach

- top, core ID 3, ordinal 1: run-enter +0us (attached=1); run-return-running +2276us (attached=1); detach-request +2314us (attached=1); progress-end-callback +70880us (attached=0); propagation-begin +70884us (attached=0); propagation-end +70898us (attached=0)
- Final states: [{'time_us': 9450373810, 'name': 'top', 'pending': '0', 'active': '0', 'attached': '0', 'ordinal': '1'}]

## builtin

- top, core ID 3, ordinal 1: run-enter +0us (attached=1); shadow-merge-applied +1319us (attached=1); run-return-running +1706us (attached=1)
- Final states: [{'time_us': 9453755162, 'name': 'top', 'pending': '1', 'active': '1', 'attached': '1', 'ordinal': '1'}]

