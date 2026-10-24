# Filter deletion Undo: latest lower input

Original15.012 criterion: 復元後に最新下層から評価. This is a bounded supporting
checkpoint, not closure of original15.011 or the earlier dependency chain.
No production change was needed; the test exercises native image layer deletion,
Undo and Redo with a Gaussian Filter, never the historical Edge fixture.

The two contexts are an ordinary root stack and a child group. Each starts with
a clean red Filter result and records its original ID and definition revision.
After deleting the Filter with native Undo enabled, the lower native buffer is
changed independently (without adding another history entry). Undo must restore
the same Filter into its exact original parent and evaluate this *latest* lower
input, not certify its pre-deletion red cache. Two Redo/delete repetitions then
exercise green, blue and yellow lower states for each context.

The selected case checks6 native Undo restorations,4 Redo deletions,8 exact
independent output pixels including initial red,unchanged item ID/definition,
and6 actual current-stack reorder subscription cardinalities. Every detached
state has stale cache-generation lineage before restoration; every restored
state executes again. This is a model/API test, not a synthetic mouse gesture
or a claim of new rendered UI evidence.

The source snapshot is committed a84cd production plus previously committed
Filter test additions and the separately sealed Options GUI candidate overlay
(options.hpp, native editor wrappers and GUI header). That candidate's exact
manifest is retained, rather than labeling the build wholly committed/latest
HEAD. No held provider candidate was copied. Eight earlier held providers and
the unchanged Clone provider remain at the committed a84cd hash; no extreme
Clone geometry/resize diagnostic is performed. Existing source seal and actual
selected sanitizer coverage are recorded separately below.

## Results

PASS native1 and selected ASan/UBSan1. Fresh builder coverage is20 instrumented
units plus53 RTTI-compatibility-only units; remaining GIMP/dependencies are not
instrumented, and leak detection is disabled. Both compile and post-run source
change lists are empty. sources.tar.gz contains113 exact recorded input files,
validated against the sealed hashes before release of the shared build lock.
Options GUI overlay manifest is explicit; no claim that unrelated aggregate
failures or later worktree states were validated here.
