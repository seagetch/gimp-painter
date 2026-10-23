# Header compilation registration evidence

Run the actual registered compiler test in a configured GIMP tree:

```sh
meson test -C BUILD painter-public-header-compilation --print-errorlogs
```

Meson builds the C/C++ static probe targets first. `--no-rebuild` is suitable only
after those targets have been built against the current inputs. Generated
sources, their source fingerprints and `header-compile-result.json` stay in the
build directory. No source-tree output or target execution is needed.

`evidence.tar.gz` has 110 UTF-8 members with exact native commands/results,
selected target metadata, compiler controls, overlay headers and diagnostics.
`evidence-manifest.json` seals each member. Executable/object binaries and the
inherited process environment are excluded. Reproducibility paths and compiler
arguments describe the native test environment; adapt paths for another checkout.

`source-duty-review.json` preserves all 92 original source identities and their
specific header checks. `tools/check_painter_header_registration.py` verifies the
current source seal, recorded outcomes and completed duty mappings. Historical
04.003/04.004/04.005 inventories/reports remain unchanged. Full runtime, link ABI
and platform behavior are outside this compile-registration result.
