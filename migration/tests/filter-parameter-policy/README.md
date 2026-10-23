# Phase B filter semantic-policy acceptance

Task `16.010/parameter-semantic-policy` separates plain legacy semantic rules
from trusted runtime `GParamSpec` metadata for the four existing isolated routes.
See [the contract](../../contracts/filter-parameter-policy.md) and
[acceptance.json](acceptance.json) for the bounded result and exact source/binary
identities. `raw-evidence.tar.gz` is one deterministic archive with its own
member-hash manifest, commands, reduced receipts, raw diagnostics and replay
recipes. It contains no full environment maps or generated test profiles.

The standalone before/after request comparison can be repeated from this repo:

```
python3 migration/tests/run_filter_semantic_policy_checks.py --output /tmp/filter-policy-normal.json
python3 migration/tests/run_filter_semantic_policy_checks.py --sanitize --output /tmp/filter-policy-sanitized.json
```

The runner extracts the immutable accepted header from commit
`c7b5f74bf520dbaeb46e93a6ce4424e1a08295ca`; it never regenerates an old pixel
oracle from the new code. Its 11,506 comparisons include admission, reservation
and exact error text for defaults, boundaries, nonfinite values, tiny legacy
float narrowing, native double, signed zero, channel flags, ROI and BPP cases.
The usual shared build/test lock is required for native checks. The archived
normal, sanitizer and minimum launch recipes show the exact commands and
selections. Native tests ran in the cloud desktop terminal namespace.

The full normal old corpus ran once. Focused native/sanitizer tests cover typed
copies, current public/hidden metadata, arrays, invalid domains, model/cache
preservation and ignored tails. The final minimum-API fixes receive targeted
rechecks, including unchanged ≥2.74 preprocessed choice tokens after diagnostic
file/line normalization. The new Blinds case proves typed/raw model retention,
not a newly executed XCF Save/reopen flow.

The minimum gate is an actual GLib 2.70.0 / GIMP 3.0.9 build in a separate prefix,
with all configured default targets and five native groups. Optional auto
features are disabled, libunwind is disabled and painter-http is enabled.
Per-process loader evidence identifies the loaded GLib/GObject/GIO files and
hashes, including the Clone subprocess. Strict undefined-symbol checks remain.
The receipt records isolated dependency/header/link selection, upstream source
and package identities, and resolved source/tooling failures.

The unsuccessful Clone warning fixture and incomplete initial process-map audit
are retained as failed attempts. Known localization/profile and occasional
plug-in Broken-pipe diagnostics remain visible. Focused sanitizers exclude the
rest of GIMP/dependencies and LeakSanitizer. Later schema-editor/persistence,
all optional features, GTK AT-SPI, tablet, other platforms and full-port gates
remain open.
