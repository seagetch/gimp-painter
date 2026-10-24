# 30.008/tool-options-delete-result

The old `g_unlink` condition at pinned afa43fae incorrectly reports success/failure. Current native `gimp_tool_options_delete` already handles success and NOT_FOUND correctly and reports other GFile errors. A distinct real caller bug remained: `gimp_tools_clear` passed NULL instead of its caller's error pointer, returning FALSE with no diagnostic. Preferences immediately dereferences `error->message`. The sole provider correction forwards `error`; no registration or dialog logic is changed.

**Causal red:** exact committed caller, real nonempty temporary options target, returns FALSE with NULL error; native regression assertion fails precisely on `'error' should not be nullptr`.

**Green:** ten native groups and the same ten groups under scoped ASan/UBSan pass. Direct deletion and caller clearing each cover regular-file success, absent target, nonempty directory, ENOTDIR and real permission denial. UID1000 is unprivileged; permission fixture makes only its temporary parent0500, checks the diagnostic and unchanged target bytes, then restores its permissions. No chmod-under-root pseudo-proof, syscall mock or actual user profile is used.

Every case checks unchanged in-memory opacity, zero property notifications and an unrelated-file sentinel. Caller success/absence suppresses subsequent normal autosave; caller failure leaves autosave enabled. Explicit always-save retains its native exception. Clear remains nontransactional across multiple tool files; no rollback guarantee is invented. No Preferences dialog rendering/UI screenshot claim is made: the error precondition and model notification behavior are tested directly.

## Source/isolation

- Private source providers from committed `06f34d8a970a9552494ddfa0aabbb1ab036afcd6`; only tools clear's NULL→error differs in green.
- Frozen linked build: exact `2ef2dd32eeb7201e8e0318f169c6005a21c10410`. All **17,096** source files, **1,322** linked inputs and **127** dynamic libraries unchanged before/after.
- Four instrumented units: ToolOptions, ToolInfo, tools and test fixture; other GIMP/dependencies uninstrumented, leak detection disabled. No whole-application sanitizer claim.
- Private HOME/XDG/TMP/GIMP3_DIRECTORY and private native helper build-output root. No shared build mutation, actual profile or bundled asset write. All source providers come from Git, never the held dirty registration file.
- Initial C++ enum cast and diagnostic matcher spelling corrections are retained as harness failures, not conflated with the causal product red.

`tested-sources.tar.gz` preserves exact before/after providers. `clear-only.patch` is the one-line product patch; parent applies it against committed index without overwriting the dirty worktree. `build.py` is the exact executed private-link harness with this execution's absolute roots and lock requirement; command/source/dependency identities are retained in `results.json`. For a new environment, reconstruct an equivalent frozen source/build and adapt roots explicitly rather than pretending this transcript is a portable rebuild.

```
PYTHONDONTWRITEBYTECODE=1 python3 migration/tests/tool-options-delete-result/validate.py
PYTHONDONTWRITEBYTECODE=1 python3 migration/tests/tool-options-delete-result/check-controls.py
# Once committed: validate exact provider bytes without reading dirty candidates.
PYTHONDONTWRITEBYTECODE=1 python3 migration/tests/tool-options-delete-result/validate.py --commit <fixed-commit>
```

Five focused receipt controls reject source/link drift, lost permission coverage, missing instrumentation and a non-failing supposed red. Original task/dependency closure remains parent-owned; no held Bucket registration task is completed by this fix. Publication remains paused.
