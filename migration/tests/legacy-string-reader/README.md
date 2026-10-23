# 10.008: original legacy string-reader observation

This closes only the original 10.008 criterion: record which representation the
actual old reader can read. It does **not** establish full-file Open, FilterLayer
procedure execution, migration-reader integration, or general XCF compatibility.

The pristine reference was rebuilt using the existing pinned recipe and all
560 checksum-verified Debian archives. Its installed GIMP 2.8.23 executable was
not overwritten. `capture.patch` appends observer entry points to the unchanged
original `xcf_save_filter_spec_value()` and `xcf_load_filter_specs()` translation
units, and adds an environment-selected path to the separate `test-xcf` binary.
The observer runs the actual old functions, not a reimplemented parser. Exact
source, adaptation, executable and evidence hashes are in `report.json`.

## Observed results

Each original-writer record contains a STRING, then an INT32 sentinel 305419896,
then END. Original writer output uses tag7. Controlled reader-positive fixtures
change **only** that tag to8; the old writer never generated tag8. These bounded
wire records are not full XCF documents and contain no user artwork.

| Input | Actual writer tag7 / original reader | Controlled tag8 / original reader |
|---|---|---|
| ASCII `reader string` | Parser false; five invalid arguments; cp40, file42 | Exact gchararray, next gint sentinel; cp=ftell=42 |
| Empty string | Parser true but premature END; invalid argument; cp16 of29 | Empty gchararray, next gint sentinel; cp=ftell=29 |
| UTF-8 `日本語` | Parser false; four invalid arguments; cp36, file38 | Exact gchararray, next gint sentinel; cp=ftell=38 |
| NULL string | Parser true and full consumption, but first argument INVALID | NULL gchararray, next gint sentinel; cp=ftell=28 |

Both empty and NULL cases demonstrate why a structurally successful old parse
is not proof of usable string recovery. All four tag8 cases independently assert
actual `GValue` type, exact string value, following-record type/value, logical
cursor and FILE position. Tag7 failures are retained, not silently repaired.

`capture.log` and `capture-repeat.log` record two executions. A third execution
using the checked-in helper produced identical observations and all eight
fixture bytes; its manifest is `replay-manifest.json`. These are successful
capture runs, not an original application test-suite pass.

## Reproduce

First run the existing `tools/build_legacy_reference.sh` using the exact GIMP,
babl, GEGL commits and legacy package lock recorded in `report.json`. Preserve
its logs, including the known GEGL simple-suite failure. The source checkout
must retain uninstrumented versions of the three capture files.

Then, from the current port repository:

```sh
python3 migration/tests/legacy-string-reader/capture.py \
  --source /absolute/legacy/gimp \
  --build /absolute/legacy/build \
  --output /absolute/new-string-capture
```

The helper rejects existing output directories and modified observer inputs,
verifies the source commit, applies the bounded patch, builds with two jobs,
runs the parser capture with a 30-second timeout, and verifies the pristine
installed binary remains unchanged. It records all commands, logs, hashes and
exit codes. If rerunning in the same observer source tree, first reverse only
this exact capture patch after preserving prior evidence; the helper deliberately
does not revert source changes automatically.

Validate retained package integrity and observations with:

```sh
python3 -B migration/tests/legacy-string-reader/test_evidence.py -v
```

These three integrity tests do not rerun the old executable. Full native build
logs and the initial observer compile failure (missing wrapper prototypes,
subsequently fixed) remain in `native-build-evidence.tar.xz`. No old parser leak
or build warning is suppressed or described as repaired. 11.007, 36.005 and other
full-file/runner gates remain incomplete.
