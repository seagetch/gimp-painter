# C32 review candidate: shared dependency schema cache

Task308 remains DOING / NOT ACCEPTED. No C32 hosted/native result is claimed.
Same-task successor retains accepted parent e25a94e21efca6e6604ddac6f15570c049f421e7.

## C31 run49 terminal observations

Run49 completed all8 jobs:5success/3failure. Exact source was published C31
7b4f411300da4be3df8012ef831f6300e5b81b28. See the preserved terminal receipt
run49-terminal-observation-summary.json, SHA256 6643500b34fa5958b68ef26baab19556c664153772bd716f4a204c68347126a7.
C31 fixed normal full107 passed, with strict icon lookup retained; the focused
fixed/historical paired mask controls are preserved. This is scoped normal proof,
not sanitizer/full task acceptance. Matched diagnostic passed the alias parsing
blocker but all14 runtime inspections then rejected schema-compiled-missing at
resource-inventory; no comparable matched timing/causal claim is accepted.
The exact failing schema directory is not exported in hosted evidence.
Sanitizer failure remains; its final details are preserved in the terminal receipt.

## Narrow preparation correction

The locked dependency helper extracts package payloads with dpkg-deb -x, without
maintainer scripts/triggers, and prepends dependency root/usr/share to XDG_DATA_DIRS.
It never generates a schema cache. C32 explicitly runs the installed
/usr/bin/glib-compile-schemas --strict against ONLY the owned shared dependency
root/usr/share/glib-2.0/schemas, once after extraction and before all4 builds,
identity snapshots and measurements; a nonempty cache is required. Compiler
failure or missing output stops preparation without retry/fallback.

All schema/source/identity predicates remain unchanged, as do native sources,
assertions, deadlines, sanitizer settings and common protected files. No system
schema root is modified. The generated dependency file is naturally captured
by all4 dependency inventories. No premeasurement asset inventory is added,
because it would warm inputs and alter the matched cache-timing policy.

## Evidence and limits

Actual installed GLib2.84.4 synthetic schema fixture reproduces the missing-cache
error, then unchanged strict schema inventory PASS and actual GSettings visibility
after compilation. This proves the fixture mechanism, not the exact unexported
hosted root or native C32 acceptance. Exact locked GTK package materialization was
unavailable in this environment; the blocked package lookup was not retried.
No ptrace/babl or GIMP native execution was performed locally.

All14 run49 failures occurred after successful source/build/plugin/catalog/font
checks. All4 exported build identities/common recipes/link policies/generator
hashes match; matched-arm source resource directories are byte-identical.
Remaining selected-data icon/theme/mime inventory and later loader closure are
not proven by current exports. No speculative policy relaxation is included.

Final source unit suite:1065 tests PASS;607 current seal entries,26 protected
common-file hashes and shell syntax pass. Independent narrow review found no
blocking issue. Fresh exact-source hosted validation remains required.
