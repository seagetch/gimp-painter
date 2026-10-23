# 08.018 prerequisite: exact native metadata policy

This four-file subset changes the legacy-adapter policy and its Python guard
tests, with this report and `evidence.json`. It establishes only a prerequisite
for original 08.018. It does not complete 08.018 or change its task/ledger state.

## Verification

The unchanged source scanner passes over 2,462 application C/C++ source/header
inputs and 683 approved object-data occurrences, retaining four dormant findings
and the same existing reviewed active construction finding. The baseline fails
only the two exact metadata sites below. Independent isolated patch replay
reproduces the integrated files and both full gate reports exactly. Baseline
39/39 and candidate 48/48 Python guard tests pass; all 39 existing test-method
ASTs are unchanged and nine targeted methods are added.

All 570 prior exact policy entries covering 681 occurrences and all prior review
records are retained byte-for-byte and semantically. Exactly two count-one
entries and two single-path provenance records are added. Other policy fields,
scanner, production source, build files, ledgers, and historical 05.014 reports
are unchanged. The historical 2,457-source/681-occurrence acceptance snapshot is
not relabeled as this successor scan.

## Narrow ownership review

- `app/widgets/gimpeditor-cxx.cpp:313` uses
  `g_object_set_qdata_full (arrow.get (), GIMP_HELP_ID, g_strdup (help_id), g_free)`.
  Native `gimp_help_set_help_data` in `libgimpwidgets/gimphelpui.c:185-197` stores
  borrowed help text. The action wrapper snapshots and later frees its temporary
  help text (`gimpeditor-cxx.cpp:380-381`); the arrow therefore owns its own copied
  native text with a matching destructor. Behavior remains in the common
  BindingStore `DropdownSlot`, with generation-checked dispatch. This rule
  permits no implementation pointer, closure, other key, or different payload.
- `app/widgets/gimppaintercompactoptions.cpp:329` uses
  `g_object_get_data (G_OBJECT (widget), "radio-button")` only to locate the
  native parent box for orientation. Native radio constructors install this
  borrowed first-button metadata in `libgimpwidgets/gimppropwidgets.c:996,1070,1259`.
  The code checks radio and box types; the native hierarchy owns the button.
  Presentation/Saved retain restoration state through BindingStore, with no
  new attachment or implementation owner introduced by this lookup.

The evidence pins each path, operation, token fingerprint, count and provenance.
Added controls cover exact sites, unknown qdata, six argument/key/payload/
destructor mutations, repeated calls, moved paths, duplicate policy entries,
unknown provenance and review-path drift. Existing lexical/dormant/alias/
malformed-source/empty-scope controls remain. The scanner is a conservative
lexical check, not full C++ semantic analysis.

## Continuity and reproduction

Base: `adfb8e289bc4eeccbd5bffa1bbe6da7840157294`. All 80 accepted source
identities and six relevant native/ASan editor-dropdown, compact-options and
libgimpwidgets binaries were rehashed and match prior accepted evidence. This
is byte-identity reuse, not a new runtime or build pass. No application or test
binary was executed; no screenshot is required for this internal policy change.

Run from the repository root:

```sh
python3 -B tools/check_painter_legacy_adapters.py --root . --report /tmp/native-metadata-gate.json
python3 -B tools/tests/test_painter_legacy_adapters.py
```

Expected: PASS, 2,462 sources, 683 reviewed occurrences, four dormant findings;
48 Python tests pass. To reproduce the two-error baseline, extract only the
policy preimage from the base commit and pass it with `--policy` while scanning
the same source. Run the old tests from an isolated directory with the unchanged
scanner to reproduce 39 passing tests. `evidence.json` records exact old/new
SHA-256, Git blob and byte identities, expected errors, full test output,
canonical full-scan commitments and all reused source/binary identities.
