# Bounded GTK DSL routing proof

This correction changes only the original `06.023` assignment. It retains the
actual Definer/Packer provider, the existing direct editor caller and its 13
reviewed support hunks. It removes that duty from 78 unrelated native GTK
model/layout hunks, and adds it to three byte-verified whole-file DSL callers:
`01.002/001641` (`gimplayerpopup.cpp`), `001643` (`gimplayertileview.cpp`), and
`001802` (`gimptooltileview.cpp`). Their feature and verification duties remain.
There are 18 resulting 06.023 assignments, all TODO at this routing stage.

The rule refines existing routes; it creates no profiles or alternative ledger
format. Direct calls require actual GLib DSL syntax, including its namespace
for unqualified calls. Comments, ordinary string mentions, includes alone and
native GTK constructors alone do not qualify. The support exceptions name only
the reviewed editor declarations, linkage, private-state relocation and two
inactive item-tree callers. This detector is a bounded source classifier, not a
C++ preprocessor or a proof of runtime reachability.

`routing-cases.json` records the reviewed public-parent identities and exact
before/after task lists. `routing-inputs.tar.gz` contains 51 source/base blobs
for the 29 affected paths. Every blob is checked against the fixed Git ID in
public parent `4f2f5de894c556ae0218049913179ac998d8c7ed`. The reproducer builds
private temporary partial trees and uses the existing `patch_sections()`
function to prove all 96 zero-context hunk ranges, scopes and payload hashes,
including the missing final newlines in the three new callers.

The reproducer uses the existing `route()`, `specification()`, task-catalog and
TSV functions. Only the implementation-task fields change in 81 hunk rows and
24 existing path summaries; the derived granularity checks and summary hashes
follow that delta. It removes exactly 78 TODO work IDs, adds exactly three TODO
work IDs and preserves every field of all 22,865 retained work rows, including
all 535 previously DONE rows and all 372 other duties on the original 93
reviewed hunks. The final work count is 22,868. Routing adds no completion claim.

From the repository root:

```sh
python migration/tests/gtk-binding/reproduce-gtk-routing.py --output /tmp/gtk-routing-output
python migration/tests/gtk-binding/test-gtk-routing.py
```

With no option, the reproducer validates and prints its report without writing
inventory files. `--output` stages the eight expected tables/summaries and a
report elsewhere. `--check` verifies the routing output and permits only valid execution-evidence
fields on these 18 exact duties; unrelated rows and source identities must stay
unchanged. `--write` first checks every inventory file
against the public parent or exact routing output, and refuses later changes,
unknown files and removed files before writing anything. Do not use it to
replace subsequent work or acceptance evidence.

The tests include native-constructor/include/comment negatives, exact support
hunk boundaries, changed source bytes, changed hunk hashes, unknown or duplicate
work IDs, incorrect work/routing assignments DONE without evidence, changes outside the 18 acceptance rows, and refusal
to overwrite later inventory edits. The separately recorded 71-file C++ caller census also
compares the detector against five positives (one provider and four callers),
eight include-only files and 58 other negative files.

Complete historical source/base trees are unavailable. This proves only the
bounded routing correction; it does not claim a full historical inventory
regeneration, feature equivalence, runtime behavior or implementation completion.
