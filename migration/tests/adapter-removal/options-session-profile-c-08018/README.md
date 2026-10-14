# Bounded actual-C controls for 08.018

This subset adds four actual-C Options/Session controls and three actual-C ImageGenerator saved-profile controls. The only production-file change documents the existing contract: optional Session `split` is valid only on TRUE and may be NULL. FALSE does not promise unchanged output.

Native and ASan/UBSan builds passed. Each final run passed all 48 cases in five ordinary Meson registrations: Options 13, Session 10, new C controls 4, profile parser 9, native profile 12 (including the three new saved-profile controls). Zero skips, failures or timeouts; no sanitizer diagnostics. Assertions remain enabled. The new files are compiled with the C compiler, not merely exposed through C linkage in C++.

The initial authored group-selection fixture failed because the native tool manager correctly prefers the current tool when it joins a group. `failed-v1.json` records that failure and its exact-precondition correction. No production defect was inferred or patched. The two corrected selection phases prove first-valid fallback and current-tool preference separately.

## Reproduction

Use the pinned Debian 13 dependencies documented in `migration/baseline/debian13/README.md`, including the recorded gimp-data submodule. Configure separate native and sanitizer builds with:

```
meson setup build-native --prefix="$PWD/prefix-native" -Dauto_features=disabled -Dlibunwind=false -Dheadless-tests=disabled -Dpainter-http=disabled -Db_sanitize=none
meson setup build-asan --prefix="$PWD/prefix-asan" -Dauto_features=disabled -Dlibunwind=false -Dheadless-tests=disabled -Dpainter-http=disabled -Db_sanitize=address,undefined
meson compile -C build-native -j 2
meson compile -C build-asan -j 2
```

Use an authenticated display and isolated HOME/XDG/cache/temp roots as in the baseline instructions. Keep parent GIMP3_DATADIR and GSETTINGS_BACKEND unset so each maintained registration supplies its own fixture. Select the extracted modern BABL_PATH and GEGL_PATH. For sanitizer execution set ASAN_OPTIONS=detect_leaks=0:halt_on_error=1:abort_on_error=1 and UBSAN_OPTIONS=halt_on_error=1:abort_on_error=1:print_stacktrace=1.

```
meson test -C build-native --num-processes 1 --print-errorlogs painter-options-session-c gimp-painter-options gimp-painter-session painter-profile-native painter-profile-parser
meson test -C build-asan --num-processes 1 --print-errorlogs painter-options-session-c gimp-painter-options gimp-painter-session painter-profile-native painter-profile-parser
python3 -B tools/check_painter_legacy_adapters.py
python3 -B tools/tests/test_painter_legacy_adapters.py
```

Do not increase timeouts or substitute skipped/headless cases for the GUI profile checks. The widget-icon fixture is generated and verifies 1,191 shipped files. Missing desktop application icons are separate inherited notices, not validated UI resources. Other retained fixture notices and coverage limits are explicit in `report.json`. LeakSanitizer and broad platform/feature acceptance are not claimed.

ImageGenerator remains an absent legacy placeholder. Its genuine options fixture is comment-only; old rate/eraser property installation was disabled. Authored unknown data tests prove preservation, bounded native rejection, archive retention and no-overwrite retries, not generation functionality. All active native obligations remain open. 08.018 stays OPEN, with 76 C-entry obligations and a separate 31 core hooks; completion remains 141/661. No perspective guard source is included.
