# Genuine legacy Blinds byte oracles

These 320 outputs were produced by the actual `plug-in-blinds` PDB procedure in
GIMP Painter at source commit `afa43fae3e920210146abed514f136fd49f671b5`.
`tools/capture_legacy_blinds.py` generated the PNG inputs and capture commands,
ran the rebuilt legacy executable, and extracted the returned PNG pixels with
Pillow. No replacement kernel or modern GEGL operation generated expected output.

The matrix is RGBA and Gray-alpha, five geometries (1×1, 2×3, 9×8, 67×66,
41×53), both orientations, four angle/segment pairs (0/1, 15/3, 67/7, 90/100),
and transparency arguments 0, 1, -1, 2. Input alpha cycles through 0, 1, 127,
and 255. The context background is RGB (31, 121, 217); the original source's
encoded luminance and rounding produce Gray 109.

All ten inputs were loaded and exported by the actual old application, and all
bytes match the generated originals, including RGB hidden below zero alpha.
The PNG save call enables preservation of transparent-pixel values. Every case
has separate start, PDB-return, and successful PNG-export markers in `capture.log`.

`fixtures.tsv` has thirteen tab-separated fields:

```
procedure width height channels angle segments orientation transparent background_r background_g background_b input output
```

Filenames are relative to this directory. `input` names actual loaded/exported
old pixels, and `output` names actual old PDB result pixels. Four channels are
RGBA8; two channels are encoded Gray-alpha bytes. Background fields are execution
bytes, triplicated Gray 109 for Gray-alpha cases.

`capture-report.json` pins the actual source and executable hashes, script,
manifest, log, raw results, PNG outputs, original input equality, and capture
command. It records a fresh isolated registry and a successful process exit.
The private profile and registry are disposable runtime state and are not part
of this evidence. The recorded timings are parent-observed log marker intervals
around the PDB call, including IPC; they are not exclusive kernel benchmarks.

The old executable emits startup GLib/GIMP critical diagnostics and an obsolete
GEGL `cache-size` property diagnostic, retained before the capture markers in
`capture.log`. Successful byte capture does not claim a warning-free legacy
startup or an otherwise fully validated legacy build. The corpus covers whole
unselected drawables, not indexed color, native high precision, selection masks,
arbitrary third-party plug-ins, or the host application's scheduler lifecycle.

To reproduce under the shared build/test lock, first source the current legacy
build environment, then use a fresh output directory:

```
python3 tools/capture_legacy_blinds.py --capture --output /path/to/new/corpus
```

The script acquires the lock itself, creates a new private profile, and discovers
only the three pinned bundled executables needed for the capture. It refuses to
overwrite an existing capture report. `--capture` may be omitted to prepare the
inputs, exact commands, and manifest without running GIMP.
