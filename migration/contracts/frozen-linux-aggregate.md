# Frozen normal Linux aggregate

`tools/run-painter-frozen-aggregate.py` creates schema-version-1 evidence consumed
by the committed `tools/package-linux-runtime.py`. A passing Meson summary alone
is insufficient. This runner does not accept imported logs or retrospectively
manufactured before-snapshots.

## Invocation

First commit the runner and all intended changes, and prepare the normal native
Linux build from that exact clean checkout, including its pinned `gimp-data`.
Run the command inside the authenticated portrait (1080 × 1920) Xvfb wrapper and
the established dependency environment. The wrapper must surround the runner;
do not add a Meson executable wrapper or edit generated Meson files. Example:

```sh
PYTHONDONTWRITEBYTECODE=1 python3 /path/to/secure-xvfb.py python3 "$SOURCE/tools/run-painter-frozen-aggregate.py" \
  --repository "$SOURCE" --build "$BUILD" \
  --expected-commit "$(git -C "$SOURCE" rev-parse HEAD)" \
  --expected-tree "$(git -C "$SOURCE" rev-parse HEAD^{tree})" \
  --output /path/outside-source-and-build/new-aggregate-evidence
```

The external wrapper must verify authorized X connections, reject unauthenticated
connections, restrict listening, and retain its own portrait-display evidence.
The runner requires `DISPLAY` and `XAUTHORITY`; their presence is not itself a
claim that authentication or geometry was verified. Never publish authority files.
Use the existing secure wrapper; the runner does not replace or weaken it.

Start the wrapper/runner with `PYTHONDONTWRITEBYTECODE=1` as shown. The runner also
disables its own bytecode writes and explicitly supplies that setting to the full
build and Meson child environments, preserving every other inherited variable.
Python helpers therefore do not leave `__pycache__` inside the committed source.
Preexisting caches/untracked files still fail the unchanged clean-source checks;
this setting is prevention, never a checker exemption or automatic cleanup.

The default lock is `/workspace/shared/gimp-painter-build.lock`; `--lock` exists
for isolated installations and fixtures, and must identify the lock used by
every other build/staging process. Do not hold that lock externally while calling
the runner. The evidence directory must be new and outside source/build trees.

## Acceptance

One exclusive lock covers the full default `ninja -C BUILD`, genuine before-seal,
unfiltered serial `meson test -C BUILD --no-rebuild --num-processes 1`, genuine
after-seal, and validation. There are no target selectors, suites, baseline
exemptions, retries hidden inside the result, or generated-Meson edits. Builds
with sanitizers/coverage or a different source directory are rejected. Both the
runner and imported package helper must match their committed bytes.
The helper import bypasses bytecode caches, including ignored timestamp-valid
stale caches.

The source commit/tree must match both explicit expected object IDs. All tracked
files, untracked files, recursive pinned submodules, preexisting ELF contents,
permissions and symlink targets are checked. Source must be clean before the
build, after the build and after tests. Working-tree blob bytes and executable
bits are checked independently of Git's stat cache; assume-unchanged and
skip-worktree entries are rejected. New ELF files fail; the only permitted
new ELF entries are verified symlinks to unchanged preexisting build ELFs within
owner-private `.GIMP3-build-config-*` test profiles. The package helper's complete
snapshots additionally seal every concrete Ninja output, configuration/command
metadata, installed inputs and source/data content, and must compare equal.

Every registered test has its original one-based registry index, suites and exact
registered/observed command. Matching uses qualified Meson names and commands;
ambiguous, duplicate, missing, additional or wrapped results fail. The gate hashes
the **raw** `intro-tests.json`; its environment-free archive has a different hash.
Both GIMP executables are independently SHA-256 sealed. The final sanitized gate
is checked with the actual package helper before acceptance.

Only `OK` plus integer return code zero is accepted. TAP `SKIP`, skipped plans,
`TODO`, failed subtests, crash/sanitizer/traceback diagnostics, criticals, assertion
or runtime-check failures, and unexpected warning diagnostics fail even if Meson
says `OK`. A short target-specific list of intentional negative-test GIMP diagnostics
and the standard welcome message remain visible as `nonfatal_warnings`; they do
not excuse a nonzero exit, a failed assertion, a crash, or a skip. Unknown GIMP
warnings fail. These rules are exercised by fixtures, not native test runs.

## Evidence and failures

`aggregate-gate.json`, environment-free `registered-tests.json` and
`test-results.json`, sanitized build/Meson console logs, and observed source/ELF
integrity evidence are retained. Failures return nonzero with `all_passed: false`.
Failed builds have no fabricated test-before facts; failed after-captures remain
missing and reject the gate. A later attempt uses a new output directory.

Raw Meson logs may contain inherited environments. They are generated in a private
temporary directory, never copied into evidence, and removed when the run exits.
Structured evidence uses field allowlists; console evidence omits environment
assignment/dump lines and redacts recognizable echoed credentials. Do not publish
unreviewed external logs, dependency shell environment files, or authority data.
SIGINT, SIGTERM and SIGHUP retain rejected evidence after stopping the child
invocation; an uncatchable SIGKILL or host failure cannot guarantee final writes.

Run fixture verification with:

```sh
PYTHONDONTWRITEBYTECODE=1 python3 migration/tests/test_frozen_aggregate_runner.py
PYTHONDONTWRITEBYTECODE=1 python3 migration/tests/test_linux_runtime_package.py
```

## Accepted normal Linux run, 2026-10-15

The exact public source `b6f0a033011492c42204a4fddb9b2ca8f691d426`, tree
`7502a59fcfd7e86d2574b2b4069f7471bc559a62`, passed all 154 registrations
under the unfiltered serial runner, with no skipped subtests, unexpected
diagnostics, or baseline exemptions. Every source/submodule/ELF/registry
integrity check passed; complete build freshness remained equal, and the real
package helper accepted the gate. The run lasted from 19:48:44 to 20:36:36 UTC.

[Sanitized receipt](../tests/frozen-linux-b6f0.json) preserves all target
classifications, visible expected negative-test warnings, executable and runner
hashes, and the full gate's SHA-256. The receipt is a summary, not a substitute
package-consumable gate. The full gate and before/after observations are retained
with the external execution evidence.

Earlier interrupted five-result attempts and the complete 153/154 rejected
run remain failures/incomplete evidence. The missing default header archives
were corrected in a separate reviewed build-graph task before this passing run.
Acceptance is for this exact normal Linux source, not subsequent changes,
sanitizers, other operating systems, installed-runtime release certification,
or complete legacy UI/full-port completion.
