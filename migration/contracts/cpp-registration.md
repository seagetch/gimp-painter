# Original 04.002: current C/C++ source registration

Checked on 2026-10-04 against production source at
`a1912fc6b4357827fc7199a0429992d006a76327` (tree
`46bc330c38603216f78ca838914c67985c6a6338`). This checkpoint changes the WBS,
source-assignment records and their checks; it does not change production
source or Meson targets.

The original acceptance condition is unchanged: custom C++ sources can be
added to Meson without recompiling existing C sources as C++. Its prerequisite
04.001 is complete. Final-link observations below support that integration
result; they do not close the separate 04.004–04.019 contracts, feature parents,
or Windows/macOS qualification. New feature sources still need registration
and their own source-specific implementation/verification evidence.

## Actual legacy input review

The old blanket `Makefile.am` rule had assigned 47 duties to 04.002: 46 exact
diff hunks across 15 paths and one menu-manifest asset. The
[47-row review](../inventory/cpp-registration-review.json) preserves every
original work ID, pinned blob and payload identity, changed text, current
replacement, correction rationale, remaining feature WBS IDs and the resulting
source/phase/task work IDs. No hunk was accepted from its filename or title.

The [input archive](../tests/cpp-registration/legacy-build-inputs.tar.gz) contains
15 source and 13 base files, fetched from seagetch/gimp-painter at
`afa43fae3e920210146abed514f136fd49f671b5` and
`a61915a8e62aad8855cd8c620bdb7195a25ffb90`. All 28 file byte identities match the
inventory's Git blob IDs; all 46 reconstructed zero-context hunk payloads match
their recorded SHA-256. The base widgets Makefile has no final newline; that
byte detail is retained and its newline-only hunk is not treated as code.

| Reviewed duty | Count | Disposition |
| --- | ---: | --- |
| Current C++ source or module registration | 19 | Registration-only DONE, mapped to actual compiled replacements |
| Final-link/archive-only entries, including empty dummy.cpp | 6 | TODO under 04.008/04.009 |
| C-only feature source/header registrations | 10 | TODO under their existing feature WBS |
| Private header-only additions | 3 | TODO under 04.005, with 04.014/04.015 checks |
| Generated PDB input/C/header registrations | 4 | TODO under 04.013/30.012/30.013 |
| Menu install manifest hunk and asset | 2 | TODO under 30.001/30.005/30.017 |
| Generated pixbuf asset header | 1 | TODO under 30.003/30.017 |
| ImageGenerator placeholder registration | 1 | TODO under 31.011/31.012, including dependency proof |
| Final newline only | 1 | Proven nonfunctional; final disposition review remains TODO under 38.001, with no implementation or removal work |

The 19 DONE duties establish registration of current modules, not complete
equivalence of every legacy source or type. Examples: presets now use native C
data/factory/view files plus a private C++ applier; brush reader/writer and
history share current modules; GTK3 popovers and canonical editors replace
several old helper TUs. The original type, vfunc, control, saved-procedure and
behavior obligations remain open under the exact WBS IDs listed in the review.
No placeholder is added merely to satisfy the old filename list.

Exact hunk exceptions in `tools/legacy_assignment_rules.py` reproduce the
corrected assignments. The full canonical TSV tables remain authoritative.
The auxiliary script checklist uses the same per-path task unions. Cleanup
review now keeps the union for mixed Makefiles instead of silently inheriting
only the last hunk (which, for widgets, was merely a newline).

The [previous work-item checkpoint](../tests/cpp-registration/prior-work-items.json)
retains all 235 work rows for the 47 reviewed source records and a digest/count
for the 22,799 untouched rows. All superseded rows were TODO; no completed
feature evidence was discarded. The checker confirms the untouched rows are
identical and every reassigned action is still TODO. The work ledger now has
22,990 rows: 112 DONE (93 previously DONE plus these 19 registrations) and
22,878 TODO. The reduction is removal of false generic build assignments, with
their correct feature/link/header/asset duties explicitly retained.

## Current build evidence

The [current report](../tests/cpp-registration/current-build.json) records source,
object, archive and executable SHA-256 identities. Full selected compiler
arguments are retained in the linked gzip command capture; no Meson environment
dump or user profile is included.

- `meson compile -C build-installed-filter -j 4` succeeded using the pinned
  Debian 13 environment and shared build lock. Its 76 steps began with generated
  Git version headers, rebuilt version/about/display C consumers and relinked
  dependent targets. It was not a no-op build. The full build output is retained
- All 2,025 compile commands were checked: 1,867 C entries use `cc`; 158 C++
  entries use `c++` and C++14. No C entry has a C++ language override
- Every registered replacement from the 19 duties, plus the focused common
  bridge and C-main entries, is present: 49 distinct current sources, 51 compile
  entries and 13 archives. Recorded objects exist and archive membership is
  checked; this is evidence from the actual app build
- `app/main.c` and `app/painter/tests/test-c-api.c` remain C. GUI, console and
  foundation final links use the C++ driver and the real `libapppainter` archive.
  Their ELF dependencies contain `libstdc++.so.6`; the apps contain the real
  `gimp_painter_binding_close` C entry
- The registered Meson `painter-foundation` executable passed all 34 cases.
  Before registering those cases, its C `main` also asserts the C→C++→C result
  `7 → 15` and exception conversion for `-1`: FALSE, the expected GError domain
  and code, and result reset to zero. Its C callback and C++ roundtrip symbols
  are present in the linked executable

Reproduce the bounded checks from the repository root:

```sh
python3 -B tools/check_cpp_registration.py
python3 -B tools/audit_legacy_granularity.py --check
python3 -B tools/check_tasks.py
```

`check_cpp_registration.py` reconstructs and reroutes all 46 changed hunks from
the frozen bytes and checks the menu asset, affected derivative ledgers, 19
DONE registration records and untouched work identities. It does not require
the shallow checkout to contain all original legacy Git objects. The general
whole-repository `assign_legacy_hunks.py --check` still requires those original
objects; this checkpoint does not claim to have rerun that broader fetch/diff.

To refresh current build observations after the authorized default build and
focused Meson test, pass `--build`, `--foundation-log`, `--build-log` and
`--output` to the same checker. Build mutation must hold
`/workspace/shared/gimp-painter-build.lock`; no GUI or feature campaign is needed
for this acceptance condition.

## Historical evidence and pending gates

The old `painter-integration.json` has six changed source hashes and remains a
historical checkpoint. It is not relabeled as a current pass. Both retained
`painter-foundation-acceptance-{native,sanitizers}.json` reports match all 24
recorded current common-foundation source hashes; they contain 34 cases,
including real reference/move/lifecycle assertions. The current Meson test above
is fresh runtime evidence; no new sanitizer or LeakSanitizer run is claimed.

The [strict-gate snapshot](../tests/cpp-registration/strict-gates.json) records
unchanged current failures: `check_painter_acceptance.py` rejects 06.028 and
07.012 because the owner-close integration reports no longer match
`filter-scheduler.cpp` and `gimpfilterlayer.cpp`; `check_clone_acceptance.py`
rejects the changed `test-gimp-clone-layer.c` hash. Historical reports and
expected digests are untouched. Source-matching common tests do not discharge
those broader tasks. Original 05.011's type-specific parent-vfunc/reentry
contract and 05.014's machine scanner remain unchecked, as do later features.

## Progress-ledger correction

The [consistency record](../tests/cpp-registration/progress-consistency.json)
enumerates 607 newly explicit TODO rows: 591 original tasks and 16 existing
children. A TODO record says that its original acceptance/dependencies remain
open; it does not claim that code is absent. The ledger now covers all 935 WBS
IDs without duplicates or checkbox/state mismatches. No slash child was added.

Only original 04.002 changes state. Original totals are 58 DONE, 11 DOING,
1 BLOCKED and 591 TODO. Existing DOING prose about normal Open, persistence,
FilterLayer UI/procedures, spill and related Clone work was corrected against
current source and its component checkpoints, while its parent states remain
unchanged. Current strict-validator failures are stated explicitly.

`check_tasks.py` now rejects a missing progress row even for an unchecked task.
The complete ledger passes; removing only TODO 04.004 from a temporary copy
fails with `04.004: missing progress row`. Existing dependency, duplicate and
DONE-evidence checks remain intact.
