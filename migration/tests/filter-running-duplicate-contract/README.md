# Actual running Gaussian duplicate isolation

Original15.010 supporting proof: 定義は複製し実行中 runner は共有しない.
Original15.009 prerequisite remains unchanged. No implementation change was
needed. This closes a specific evidence gap: the old freshness test supplied
RUNNING as loader snapshot metadata; here the actual scheduler is observed in
RUNNING with one real Gaussian worker start. No synthetic state injection is used.

The original Filter is512×512 in a red-source group. It is duplicated while
actually RUNNING (verified before and after duplication without main-loop
advancement). The duplicate starts with zero worker count and is not RUNNING.
The original's real current generation is cancelled, then the duplicate is
inserted over an independent blue-source group. It must start its own worker and
produce blue, not import the original red worker's result or inherit cancellation.

Raw definition bytes and Gaussian radius20 survive independently. Installing
radius5 and no raw bytes on the original leaves duplicate generation/run count,
radius20 and original raw bytes unchanged. The original produces red. Updating
only the duplicate's own lower source produces green with one additional start;
the original stays red. Four independent exact output pixels and native counters
establish the separate execution/input paths. Immutable saved argument models
may share immutable storage; independent definition replacement and runtime
scheduler ownership, not deep-cloning immutable memory, is the criterion.

This is a model/API test, not a new rendered UI claim. No old Edge/string or
watercolor runtime is invoked. No held popup/Canvas/preview/Bucket lifecycle or
Clone extreme-geometry diagnostic is invoked. The source base is exact20b2204b11
plus the separately hashed three-provider Options mode overlay and two owned
Filter test files, not evolving main worktree providers. Its overlay manifest is
retained explicitly; aggregate results and candidate-scoped results stay separate.

## Results

PASS native2 selected cases (new active duplicate plus committed restore
regression),PASS focused ASan/UBSan1 active duplicate case. Fresh instrumentation
covers20 units plus53 uninstrumented RTTI-compatibility units; remaining
GIMP/dependencies uninstrumented,leak detection disabled. Both input change
lists are empty. sources.tar.gz seals116 recorded input files,all hashes
validated under the shared build lock before release.
