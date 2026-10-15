# Native test source-profile isolation

`app/tests/tests.c` treats the checked-in `app/tests/gimpdir` directory as
read-only input. Configuration loading still occurs against the original
source directory. Ordinary personal RC files remain independently owned, and
explicit custom-system-RC calls retain their existing personal-RC semantics.

Immediately after configuration loading, both public test initialization paths
replace a source-fixture `GIMP3_DIRECTORY` with a fresh private directory under
the runtime launcher's `TMPDIR`. This happens before core initialization and
restore, GUI activation, or returning the instance to the test. The core-only
initializer also needs this boundary: `painter-layer-ui` creates GUI widgets
after calling it, and those widgets can write color-profile history.

The private directory receives copies of `sessionrc`, `dockrc`, and `tags.xml`,
plus any basenames selected through `GIMP_TESTING_SESSIONRC_NAME` and
`GIMP_TESTING_DOCKRC_NAME`. Missing optional input files retain the normal
startup fallback; other copy errors fail the test. These are copies, never
links into the source tree. Generated `theme.css`, `profilerc`, personal RCs,
plug-in caches, and unrelated files are not imported from previous runs.
Selected fixture names must be nonempty basenames: dot/dot-dot, absolute names,
either slash direction, and Windows drive/stream colon forms are rejected on
every platform before the private directory is created or any child is copied.
Reserved Windows device basenames (including extension and numbered-device
forms), trailing spaces/dots, reserved punctuation, and control characters are
also rejected. See Microsoft's [file naming rules](https://learn.microsoft.com/en-us/windows/win32/fileio/naming-a-file).
Invalid input terminates with failure rather than invoking an assertion abort.
Ordinary Unicode basename characters remain allowed.

The core-only initializer retains an explicitly selected non-source profile.
The GUI initializer still selects the source directory for configuration
loading, as it did previously. Tests that deliberately switch profiles after
the GUI initializer returns, including the canvas native-session roundtrips
and device-save lifecycle cases, still control their own paths.
The activation callback's existing shutdown redirect to build-tree
`app/tests/gimpdir-output` also remains. The private directory is removed at
normal process exit, after checking the owned RC autosave idles; the isolated
launcher owns cleanup after assertion aborts. Recursive cleanup does not follow
directory symlinks.

## Regression coverage

`gimp-test-config-fixture` now exercises:

- Byte-preserving copies of the ordinary source fixtures and explicitly named
  Unicode session/dock alternatives, without changing the selected basenames.
- Real `themes_init()` output and real `GimpColorProfileStore` disposal output
  in the private profile, while source output sentinels remain unchanged.
- Repeated subprocess initialization against the same source and build roots,
  distinct private profile paths, and process-exit cleanup.
- Direct core-only initialization through `gimp_init_for_testing()`, including
  initialize/restore, followed by the actual theme/history writers. An empty
  test-owned plug-in directory keeps these profile tests independent of
  unrelated installed or parent-build plug-ins.
- A caller-selected non-source core profile that remains selected and survives
  process-exit cleanup, with its existing marker intact and writer output in
  that exact directory. Source fixtures are not copied into this profile. The regression seeds its own
valid empty tag cache and an empty source template list for real core restore;
these caller-owned inputs survive unchanged.
- Rejection controls for both session and dock environment variables, covering
  30 invalid name forms each. Each subprocess must report the specific basename
  rejection and fail before creating a profile. A real source file reachable
  through `../escaped-profile` and a destination sentinel outside the would-be
  private profile establish that no escaped overwrite occurred; source fixtures
  and the independent caller-owned profile marker are also checked unchanged.
- The existing private RC autosave, build plug-in cache, and explicit custom
  RC contracts, unchanged.

The source-path setter audit found source fixture selections before the public
initializers, with no ordinary test resetting the directory to source after
initialization. Compatibility session helpers read their expected files
directly from source before initialization, then redirect writes to the build
tree. Existing canvas-session tests remain the full-activation coverage for
intentional post-initializer directory and basename overrides.

## Validation status

Native build/install and 14 registrations passed, covering 281 TAP cases
plus all 60 invalid-name controls. The final 7-case profile fixture has empty
stderr. Core, layer UI, canvas/docks/session/input, compact options, navigation,
profile migration, and device-save lifecycle regressions pass. All 13,516
source files remained byte-identical across the first regression group; the
only subsequent change was the deliberate fixture-input correction. No test
created source output. See [receipt](../tests/native-test-profile-isolation.json).

The initial 7-case run passed assertions but reported missing synthetic
`templaterc` and tag-cache inputs; it is retained as non-clean evidence. The
fixture now supplies valid empty test-owned inputs and preserves them. Existing
application-icon/plug-in-enumeration notices, device/bus notices, and four
expected negative-case GIMP warnings remain recorded. The existing strict
aggregate diagnostic classifier reports no unexpected diagnostics or TAP skips. No source cleanliness checker, exclusion rule, production
writer, or warning policy was weakened. The final frozen aggregate,
sanitizer refresh and cross-platform acceptance remain open.

Run the aggregate with `PYTHONDONTWRITEBYTECODE=1` so Python imports do not create
untracked `__pycache__` files in frozen sources. This is an execution setting,
not permission to ignore generated source files in the cleanliness check.
