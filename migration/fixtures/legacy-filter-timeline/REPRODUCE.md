# Reproduction and staging

## Validate the existing seal (no GIMP execution)

From a repository containing the delivered migration tree:

    PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s migration/tests -p 'test_legacy_filter_timeline.py' -v

Python 3 standard library only. These tests check the sealed bytes and relationships from the actual raw logs. Passing them is not a new application run or a bit-identical rebuild.

## Create a new measurement in this captured environment

Required siblings under /workspace/scratch/5b5281e79681 are the pinned configured legacy-filter-positive-20261015/gimp source/build, its build/prefix original application and plugins, deps/root, and build/env.sh. This package does not contain those binaries or all dependencies. The original build recipe and signed dependency archive provenance are retained in the maintained tools/build_legacy_reference.sh and migration baseline records; this capture specifically reuses the already-built archives and documents their hashes, rather than claiming a clean independent build.

Use a new nonexistent output directory. Never modify the installed prefix. Bash, GNU Make, Git, timeout, sha256sum, Xvfb, xauth, xdpyinfo, ss and Python 3 are required, together with the recorded dependency environment. The exact bundled run_authenticated_display.py creates a bounded loopback-only listener with private X authentication and verifies rejection without the cookie; no security settings or -ac are used.

    W=/workspace/scratch/5b5281e79681
    F="$W/legacy-filter-timeline-20261015/deliverables/migration/fixtures/legacy-filter-timeline"
    OUT="$W/filter-timeline-replay"
    test ! -e "$OUT"
    mkdir -p "$OUT"
    cp -a --reflink=auto "$W/legacy-filter-positive-20261015/gimp" "$OUT/gimp"
    for f in capture-filter-timeline.c instrument.py build.sh run.sh collect-provenance.py package.py run_authenticated_display.py README.md summarize.py REPRODUCE.md; do cp "$F/$f" "$OUT/$f"; done
    python3 "$OUT/instrument.py"
    bash "$OUT/build.sh"
    bash "$OUT/run.sh"
    bash "$OUT/run.sh" repeat-captures
    python3 "$OUT/package.py"
    mkdir -p "$OUT/deliverables/migration/tests"
    cp "$F/../../tests/test_legacy_filter_timeline.py" "$OUT/deliverables/migration/tests/"
    PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s "$OUT/deliverables/migration/tests" -p test_legacy_filter_timeline.py -v

package.py invokes the exact bundled summarize.py to regenerate MEASUREMENTS.md from the new raw-log records.

instrument.py restores only three original target files from pinned HEAD before adding the documented observation-only hooks and new header. build.sh installs the exact bundled capture-filter-timeline.c in this isolated source and rebuilds core + test-xcf. It does not invoke make install. The source-overlay.patch records the COMPLETE inherited build adaptations plus current instrumentation/test change against pinned original source. Apply that complete patch only to clean pinned source, then copy legacy-timeline.h to app/core/; do not apply it over an already adapted build. patch-check.log validates the patch against clean pinned versions of every changed file.

collect-provenance.py and package.py are the exact exporter/helper, not pseudocode. They capture actual hashes, dependencies, logs, and parsed timeline; runtime-byte-inventory.json records current postcapture edge and loader-resolved shared-library bytes only, and cannot prove their historical before/after equality; exporter execution produces a NEW seal and must not be described as checking the old one. Absolute paths in build diagnostics and hashes are historical paths. New elapsed times, pointers and even race outcome details can differ. Expected original failure behavior is evidence, not a reason to alter the old implementation.

## Parent staging only

Copy deliverables/migration/fixtures/legacy-filter-timeline into the maintained migration/fixtures tree and deliverables/migration/tests/test_legacy_filter_timeline.py into migration/tests. Run that test and repository task checker. Parent independently reviews acceptance and updates tasks.md / migration/progress.tsv. This worker has not edited either ledger or published a commit. 02.013 acceptance covers the retained original event ordering and bounded outcomes only, with GUI/headless distinction and detached failure explicit.
