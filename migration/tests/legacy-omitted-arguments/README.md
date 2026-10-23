# 10.009: actual legacy omitted argument observations

The pinned old writer and reader were executed, not reimplemented. The exported
observation wrappers are appended after the original source bytes; their hashes,
patch, exact commands and native output are retained. The installed pristine
GIMP 2.8 reference binary is unchanged. See `manifest.json` and `report.json`.

For each of INT8ARRAY (tag 6), FLOATARRAY (tag 7), and DRAWABLE (tag 10), three
distinct inputs produce identical 28-byte files: tag/zero-size, a following
INT32 sentinel `305419896`, and END. Actual old parsing returns an **INVALID**
first GValue and the correct following `gint`, consuming exactly 28 bytes.
Array elements/length/empty-versus-nonempty and drawable IDs are not serialized
and cannot be recovered. The original wire tag/slot position, omission, and
following scalar are recoverable; the omitted slot is not a usable argument.

FLOATARRAY's output is byte-for-byte identical to the actual NULL string writer
fixture in `../legacy-string-reader/`; tag 7/size 0 must remain ambiguous. This
is not proof of full-XCF Open, procedure execution, or drawable resolution.
Other array GTypes have no typed writer branch and fall through to unsupported
tag 1, losing type/content. The original tag-1 pointer-copy bug is documented
in the compatibility contract and is deliberately not executed here.

## Reproduce

After `tools/build_legacy_reference.sh` succeeds, use an uninstrumented pinned
source tree and new output directory:

```sh
python3 migration/tests/legacy-omitted-arguments/capture.py \
  --source LEGACY_SOURCE --build LEGACY_BUILD --output NEW_OUTPUT
python3 -B migration/tests/legacy-omitted-arguments/test_evidence.py
```

The helper builds only the observer with two jobs and limits execution to 30s.
It refuses overwritten evidence and modified input sources. The native archive
retains the initial six-case successful run, the empty-array observer setup
failure (nonnull pointer/zero length), and corrected nine-case success. The
fix is only to wrapper input construction (NULL/zero length), not legacy code.
All fixtures are bounded synthetic bytes. No user images or personal data were
used. The evidence tests verify retained artifacts, not substitute for native
execution. Legacy parser leaks are not repaired or certified by this work.
