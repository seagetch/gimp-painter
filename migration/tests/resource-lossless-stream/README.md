# Resource losslessness, explicit defaults and output-stream contract

Bounded evidence for original 19.004 (default versus explicit switches), 19.005
(text/reference preservation), 19.007 (unknown-key retention), 19.011 (settings
and curves), and 19.012 (metadata and references). No production change was
needed. Their prerequisite chain remains open; this does not close original
rows or the held native icon/preview/save-provider obligations.

The independent recursive JSON-tree oracle compares object member sets, array
order, nulls, booleans, strings and numeric values. INT64 comparisons are exact;
mixed double/INT64 comparison checks finite integral range before conversion.
Fixtures cover signed 2^53+1, INT64 limits and nested wide integers against
explicit integer constants; equivalent small integer/double values still match. Expected trees use JSON-GLib
operations, not Resource getters/serialization. Tests retain nested unknown
members while editing known settings, curves, switches, paper references,
group and parent names; copies remain independent. All five switch keys have
absent/explicit false/explicit true cases; both text keys have absent/null/
empty/Unicode cases (23 scenarios), checked again after encode/redecode.

A custom real GOutputStream accepts only seven bytes per write. Five cases
cover complete short-write output, immediate failure, failure after 17 bytes,
pre-cancellation and cancellation after 14 bytes. Checks require exact error
codes, exact partial prefixes, no ownership-taking close, and unchanged
Resource state. Stream prefixes are not rolled back after failure: no atomic
external-file publication claim is made. Four malformed known-field cases
must reject. Unknown opaque data remains permitted.

All four groups pass normal C++14 and ASan/UBSan builds of Resource, error and
test translation units. GLib/JSON-GLib and other dependency libraries are not
instrumented; LSan is disabled. Four compiled production-source mutants
(drop vendor object, convert retained INT64 to double, default absent switch true, hide stream failure) fail
runtime assertions in their respective groups. No old runtime, engine,
native brush provider, UI, whole application or arbitrary allocation-failure
claim is made. Input-stream faults and icon consistency are not tested here.

Two retained preliminary failures are test-harness mistakes, not product
regressions: a missing JSON fixture brace and a ternary passed unparenthesized
to a GLib assertion macro. The final matrix corrects both.

Replay: `python3 migration/tests/resource-lossless-stream/run.py --output /tmp/resource-lossless-stream`.
The script compiles privately against the locked dependency root; it neither
modifies nor builds the clean aggregate source. Generated metadata headers are
included and source-hashed. `validate.py --check-source` replays receipt/source
checks without native execution.
