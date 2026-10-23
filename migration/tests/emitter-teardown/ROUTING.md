# Bounded original 07.008 routing proof

This correction addresses only the original `07.008` acceptance condition:
`Connection` destruction must not access a freed emitter. It reviews the exact
94 existing source-specific verification duties at public parent
`1f740118fadbccf464f83d2d621645d993025b02`. It creates no new task, profile or
ledger format, and assigns no source-duty completion.

Four duties remain:

- `01.002/000042`, `app/base/delegators.hpp`: the common legacy `Connection`
  provider owns the borrowed-target destruction/disconnection contract
- `01.002/001604`, `app/widgets/gimpeditor-cxx.cpp`, hunk 1: the editor dropdown
  reaches `decorate_popover` and its `PopoverDecorator` connection owner
- `01.002/001544`, `app/widgets/gimpcontainertreeview.c`, hunk 8: popup-cell
  activation reaches the popover's cancel/confirm connection owner
- `01.002/001651`, `app/widgets/gimplayertreeview.c`, hunk 7: popup-cell renderer
  construction reaches `decorate_popover`

The other 90 hunks cover declarations, inactive callers, native C signal
handlers, or unrelated GTK model/layout work. Their source-specific reasons
and exact before/after task lists are in `routing-cases.json`. Those reasons
remove only this destructor verification assignment. Their implementation,
feature, native signal, and other lifetime obligations remain unchanged.
In particular, the `07.008/untracked-signals` and `07.008/connection-name`
implementation duties on the common provider remain open at this routing stage.

`EMITTER_TEARDOWN_TRIGGERS` limits the existing `widget-helpers` and `cpp-gtk`
verification routes to the three exact reviewed trigger hunks. The common
`cpp-signals` route is unchanged. No existing profile contents change. This is
an exception map for fixed historical hunks, not a general call-graph analyzer.
The old dropdown API has no direct surviving equivalent established here;
shared `Connection` safety does not prove dropdown, preset, cell-renderer or
other feature equivalence. Separate runtime acceptance is recorded elsewhere
in this directory.

## Fixed-source reconstruction

The reproducer reuses the committed `gtk-binding/routing-inputs.tar.gz` and
`gtk-binding/caller-census.tar.gz` archives; it does not download a new tree or
trust unverified excerpt text. It checks both archives against the public
parent, selects 49 needed source/base blobs across 27 paths, and checks every
blob's Git identity against the existing fixed inventory. It then creates
private temporary partial trees and invokes the existing `patch_sections()`
parser to rebuild exactly 94 zero-context hunks. Every range, kind, scope and
payload SHA-256 must match the reviewed cases and public-parent inventory.
The common provider comes from the existing caller census archive.

The corrected assignments come from the actual `route()` implementation.
The work rows come from the existing `specification()` and public-parent task
catalog. The proof rejects any route change outside the original `07.008`
verification task. It preserves every field in all 22,778 retained work rows,
including all 569 previously DONE rows and all 302 other duties on the reviewed
94 sources. The removed set must be exactly 90 original TODO verification
work IDs. No new work ID or DONE row is generated.

Only six existing inventory files change:

- `hunk-wbs.tsv`: the verification-task field on 90 rows
- `cleanup-candidate-review.tsv`: the verification-task field on 23 existing
  path summaries, following the established last-hunk and special-disposition
  conventions; these summaries do not replace the exact per-hunk duties
- `legacy-port-work-items.tsv`: removal of the 90 reviewed TODO rows
- `granularity-review.tsv`: verification counts and work IDs on those 90 sources
- `wbs-assignment-summary.json`: the hunk-assignment ledger hash
- `granularity-summary.json`: verification actions decrease from 6,869 to 6,779;
  the 15,999 implementation actions and other summary fields are unchanged

`changed-hunks.tsv`, its hash, every asset assignment and all other inventory
files remain byte-for-byte unchanged. `routing-proof.json` records the expected
pure routing hashes, the four retained IDs, and the 90 removed IDs. Subsequent
valid execution evidence on the four retained rows changes the work-table
hash, so that recorded pure-routing hash is not a claim about later acceptance.

## Reproduce and verify

From the repository root:

```sh
python migration/tests/emitter-teardown/reproduce-emitter-routing.py --output /tmp/emitter-routing-output
python migration/tests/emitter-teardown/test-emitter-routing.py
python migration/tests/emitter-teardown/reproduce-emitter-routing.py --check
python tools/audit_legacy_granularity.py --check
```

With no option the reproducer validates and reports without changing inventory.
`--output` stages the six tables and a report outside the real inventory.
`--write` first checks every inventory file against the exact public parent or
pure routing output. It refuses later edits, completed evidence, added files
or removed files before writing anything. It is safe to repeat only before
later execution work. `--check` verifies the complete inventory and allows
valid execution-evidence fields only on the four exact retained original
`07.008` rows; every source/action identity and every other row must remain.

`routing-tests.log` records 15 passing controls. They check all four positives,
adjacent-hunk/declaration/DSL negatives, unchanged common signal duties and
profile contents, changed source/base bytes, changed hunk hashes and scope,
correlated manifest/hash tampering against reconstructed source, missing
coverage, unknown/duplicate/misassigned work, attempts to discard DONE evidence,
wrong routes, incomplete DONE evidence, changes to another duty on a retained
source, row additions/reordering, and refused overwrites after later work.
Synthetic checks on other indexed paths exercise profile/header isolation;
they are not presented as verification of unavailable historical source.

## Historical GTK proof compatibility

The earlier `gtk-binding` proof reconstructs the historical `06.023`
implementation correction from parent `4f2f5de894c556ae0218049913179ac998d8c7ed`.
Its reproducer now validates and preserves its reviewed historical verification
fields directly instead of comparing them to today's independent verification
routing. All original implementation and source checks remain. A new regression
shows that the current `07.008` change leaves all eight historical output hashes
exactly equal to the original recorded `gtk-binding/routing-proof.json`.
`historical-gtk-routing-tests.log` records all 13 passing tests. This does not
claim that the old historical `--check` accepts the later current inventory.

Complete historical trees are unavailable. No full historical generator pass,
feature equivalence, platform acceptance or source-duty completion is inferred
from this bounded routing proof.
