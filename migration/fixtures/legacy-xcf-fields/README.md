# Genuine legacy saved-field fixtures

The original pinned gimp-painter 2.8.23 PDB and XCF writer produced both files.
`manifest.json` records source commit, unchanged writer/reader/text source hashes,
installed executable hash, baseline build report hash, script and fixture hashes.
The capture does not patch the old writer or manufacture old XCF bytes.

- `rgb-fields.xcf`: editable text, RGB ordinary layer/mask/channel, custom unit
  with historical five-string framing, locks/links, persistent metadata, guides
  and resolution
- `indexed-fields.xcf`: four exact RGB palette entries, indexed-alpha layer and
  built-in millimetres

`capture.scm` is relocatable stimulus; replace only `@OUT@` with an authorized
output directory. `capture-executed.scm` records the exact capture invocation
input. The headless `gimp-console-2.8 --batch-interpreter=plug-in-script-fu-eval`
process completed both saves and emitted the completion marker in `capture.log`.
Warnings are retained. These are old-writer fixtures; an old-reader reopen was
not part of this capture. Modern Open/edit/Save/reopen assertions live in
`app/tests/test-painter-xcf-fields.inc` and exercise the actual application path.

The fixture scenes contain no private artwork or external personal data. They
were generated for these migration tests and are distributable under the
repository's GPL-3.0-or-later terms.
