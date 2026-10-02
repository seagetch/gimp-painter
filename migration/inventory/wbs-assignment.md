# Complete legacy-delta WBS assignment (01.013)

The fixed legacy diff contains **2,489 zero-context text hunks or binary/metadata
sections in 947 changed paths**. `hunk-wbs.tsv` assigns every section, including
sections whose text occurs in upstream GIMP 3. `asset-wbs.tsv` independently
assigns **402 asset/configuration/translation/manifest paths**, including all
177 `.myb` definitions, 178 brush PNGs and the compressed preview template.

Each assignment retains the original `01.002` identifier, exact source/base
blob, source range and SHA-256 of the signed diff payload. It names concrete
implementation WBS IDs and independent verification IDs, not just section
numbers. The original `changed-hunks.tsv` feature/disposition columns now point
to the same actions. Asset identities stay separate from executable code.

`tools/legacy_assignment_rules.py` is the reviewed routing contract. It has
explicit exceptional-hunk routes for mixed files and bounded file families for
repetitive controls, packaging and assets. An unknown path, changed hunk range,
missing source section, undefined WBS action, missing verification action or
changed generated output fails validation. Whole-file additions may require
several independent actions; this is explicit in the task list, not a claim
that a whole file is one implementation step. The subsequent 01.017 work-item
checklist expands those actions before the inventory gate closes.

## Newly explicit work

- `29.010/standard-tool-options`: preserve the horizontal/compact controls of
  every affected standard tool; a shared editor must not silently drop them
- `30.008/tool-options-delete-result`: the legacy `gimptooloptions.c` hunk 7
  changes `g_unlink (...) != 0` into `! g_unlink (...) != 0`. Do not propagate
  the reversed success/error test; cover success, ENOENT and genuine errors
- `30.017/brush-preview-generator`: preserve the brush-label/preview script's
  generated asset purpose using a verified ImageMagick path

The late startup and exhaustive-vfunc verification children were moved to
`30.016/feature-entry-point` and `38.004/all-vfunc-exception-containment` so a
future implementation cannot be required to finish its own foundation gate.
Their contracts and existing references are preserved.

## Verification

Run from the repository root:

```
python3 tools/assign_legacy_hunks.py --check
python3 tools/check_tasks.py
```

The generator re-reads the pinned Git diff, checks exact section-set equality,
recomputes hashes and validates every route against the WBS. DONE here means
assignment complete only. **All implementation and behavioral checks remain
TODO**. No file has been declared safe to delete, no upstream text match has
been elevated to behavioral equivalence, and no runtime/round-trip result is
claimed by this inventory.
