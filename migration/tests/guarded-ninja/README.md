# Guarded Ninja metadata preparation

The maintained `tools/prepare-guarded-ninja.py` supersedes the uncommitted
historical `native-setup/prepare-guarded-ninja.py` retained in the October 2026
recovery archive. The old helper used unrestricted `pickle.loads` on every
`meson_exe*.dat` before selecting generator commands. Do not use that archived
helper for a new preparation. Its archive is retained unchanged as evidence.

## Scope and compatibility

The replacement does not import Meson or execute generators. It checks pickle
opcodes before a restricted reader maps exact Meson names to inert local records.
It rejects reducers, extension-cache lookups, persistent IDs, unknown globals,
unsupported fields/types, cycles, trailing data, oversized raw or expanded data,
and unsupported protocols. A schema-only protocol-4 writer preserves execution
state and rewrites only command arguments. Checks remain enabled under `python -O`.

The supported instance schema is the repository-pinned Meson **1.7.0** native
`ExecutableSerialisation` and `EnvironmentVariables` schema, verified against
[upstream core.py](https://github.com/mesonbuild/meson/blob/1.7.0/mesonbuild/utils/core.py)
(blob `a87f77acc14feaa45cd30b09d3c6dad40dc7851c`). Protocols 4 and 5 are accepted.
Executable wrappers and generator-specific environment changes are unsupported.
An unfamiliar Meson schema fails closed rather than relaxing deserialization.

`arguments.json` preserves all 14 generator argument arrays from the retained
command audit, with source/build locations normalized to `/fixture/source` and
`/fixture/build`. Seventeen argument strings contain literal newlines. Unit
fixtures use independently authored inert classes with the inspected Meson field
layout; standard pickle decoding checks the newly emitted fixture output.
This is schema/wire-format compatibility testing, not a test of installed Meson
or original `.dat` files. No original or downloaded metadata was deserialized.

## Replacement preparation command

The old zero-argument invocation depended on a temporary directory layout.
To prepare a graph, use the explicit maintained CLI:

```sh
python3 tools/prepare-guarded-ninja.py \
  --source /work/gimp-painter \
  --build /work/build-http \
  --output /work/guarded-http-new \
  --expected-recipes 17
```

All paths must be absolute simple paths, with no symlinks, parent traversal,
whitespace, or shell/Ninja metacharacters. Source, build and output must be
separate, non-nested trees; the output parent must exist and the output directory
must not exist. Non-regular metadata/source inputs and ambiguous source-path
prefix matches are rejected. The graph appears at `OUTPUT/http.ninja` only after
preparation succeeds; a failed attempt may leave a partial directory but no graph.
Use another fresh output path after investigating a failure. The original source,
metadata and `build.ninja` are not modified.

This command prepares files only.
The graph and executable commands still need a trusted provenance and review;
this helper is not a general Ninja sandbox, a concurrency/hostile-same-user
filesystem boundary, or proof that every possible graph command avoids source
writes. Its expected pinned case is 14 serialized generators plus 3 direct
generator recipes. The recipe count is an explicit compatibility check.

## Verification

```sh
python3 tools/test_prepare_guarded_ninja.py
python3 -O tools/test_prepare_guarded_ninja.py
python3 -m py_compile tools/prepare-guarded-ninja.py tools/test_prepare_guarded_ninja.py
```

Tests use only local benign synthetic records, inert unsupported-opcode fragments,
temporary files and mocked Git source listings. They cover ordinary schema and
multiline argument round-trips, all retained argument arrays, metadata rejection,
raw/expanded limits, source-prefix collisions, symlinks, directories/FIFOs, output
isolation, traversal and disabled-assert execution. They do not run a native
build, GIMP, a restored package, downloaded code, or a malicious payload.

Native GTK/build verification was not rerun. Task **08.012 remains 135/661**;
this helper fix does not advance its
acceptance count or change deferred external-library findings.
