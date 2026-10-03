# SmallTiles evidence, isolated route

This is a compact **derived subset**, not a new legacy capture. The binary
archive contains only SmallTiles raster evidence plus original provenance
reports. No Retinex implementation, Retinex raster, copied build log, or copied
capture log is included. The complete original reports still describe their
mixed historical captures; their dates, statuses, counts and hashes are never
rewritten to imply that they describe the current tree.

The archive SHA-256 is
`d28e4b74436f980a9e263225748b056df2228ed6b5d8b815f4b6e1c1d67f8695`.
Its manifest links each preserved file to its original archive/member and
SHA-256. A derived index has an explicit selection rule and the source
member's hash. `provenance/` holds the unchanged source report bytes and
unchanged public/private argument metadata. The derivative retains these
three exact source archive identities:

| Original archive | SHA-256 |
| --- | --- |
| legacy-additional-filters.tar.gz | e3e3b804e76693ddfd27da381a0e2bab92eba7b6fe1b5d5f44c8ea62dbf076ae |
| additional-filter-plugin-evidence.tar.gz | 59445d1797a8d36b02575e3f8458aea0e886d44b86fbed56ac5a88a9df9c3bf1 |
| legacy-filter-additional-live.tar.gz | 6fce7cff0b6d19bd3867ac305c3844f67c72ee41ed18525390ab48985352db47 |

## Contents

- `pdb/`: all **196 genuine legacy PDB outputs**, factors 0 through 6,
  RGB/RGBA/Gray/Gray-alpha, seven geometries including 1x1, non-square and
  non-divisible dimensions. Inputs, original PNG inputs, outputs and an index
  are retained. `fixtures.tsv` is the standalone helper test input
- `public-before/` and `public-after/`: the **24 SmallTiles cases** from each
  historical mixed public 56-case capture. Their bytes and complete SmallTiles
  argument metadata are equal
- `hidden-offtree/` and `hidden-integrated/`: the **196 SmallTiles cases** from
  each historical mixed hidden 370-case capture. After the explicitly tested
  old zero-alpha shadow-merge repair, both match all 196 old PDB outputs
- `live/`: **25 actual old FilterLayer scenes**, **48 actual old merge records**,
  all source/start/shadow/mask/final/rerun bytes, plus selection bounds/events.
  Twenty-four scenes cover RGB/Gray, factors 0/1/3/6 and no/hard/soft selection;
  the final scene records a nonintersecting selection and a changed lower
  source. There are two expected outputs per scene, or 50 native comparisons

The original legacy commit is
`afa43fae3e920210146abed514f136fd49f671b5`. Original source, executable,
initializer, instrumentation and capture-tool hashes remain in the original
reports. This subset does not claim a new capture or new source authority.

## Verify and reproduce

From the repository root:

```sh
python3 tools/check_small_tiles_evidence.py
python3 tools/check_small_tiles_evidence.py --extract /tmp/new-empty-small-tiles-evidence
```

Extraction requires an empty destination and occurs only after the archive,
member bounds, exact file hashes, provenance, derived records and historical
cross-comparisons pass. The result is
`/tmp/new-empty-small-tiles-evidence/small-tiles-evidence`.

To regenerate identical bytes into a new location while retaining the original
captures read-only:

```sh
python3 tools/derive_small_tiles_evidence.py \
  --source /path/to/original/migration/fixtures \
  --output /tmp/new-small-tiles-evidence.tar.gz
```

The generator refuses to replace an existing archive or manifest.

## Current implementation checks

Historical checks alone do **not** establish current-tree behavior:

- `app/painter/tests/test-filter-procedure.cpp` reads the verified root from
  `GIMP_PAINTER_SMALL_TILES_FIXTURES`, runs the actual isolated helper for all
  196 PDB rows and requires byte equality. The original Blinds tests remain
  unchanged; the existing optional command-line Blinds fixture still works
- `app/tests/test-filter-small-tiles.inc` performs the 50 actual-old native
  comparisons and separate analytic lifecycle probes: factors 0/1/3/6 lower
  updates, invalid domain/type cache preservation, running replacement/cancel,
  no-merge lower publication, late selection/component/own-alpha-lock context,
  dependency updates, owner close/resource reuse, and saved editable definition
  reopen with subsequent nonuniform lower update and undoable factor 0↔1 re-edit
- `tools/capture_small_tiles.py --build BUILD --output NEW_OUTPUT` captures the
  current actual public 24 cases and hidden 196 cases in a fresh registry. It
  checks public bytes and signature unchanged, hidden range 0..6 and two
  calling-error rejections for interactive/last-values invocation. Its caller
  must hold the shared native/build lock. The report pins the actual binaries,
  source, fixture archive, observations and log used by that current run

The 196 old-PDB cases, 25 live scenes/48 merges, 220 current valid PDB invocations,
2 rejected PDB invocations and analytic lifecycle cases are distinct counts.
Keep their results separate when recording acceptance. This evidence does not
close other procedures, other precision/transfer domains, total-memory/latency,
GUI progress, platform, packaging or release gates.
