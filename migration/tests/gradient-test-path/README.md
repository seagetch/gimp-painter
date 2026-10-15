# In-build test gradient search path

When GIMP_TESTING_ABS_TOP_SRCDIR is set, the test default searches only source
data directories, while the Custom gradient writable directory still points
at the isolated profile. The wrapper-created gimprc previously configured only
Script-Fu, so all 13 direct-GIMP tests reported Failed to save data at shutdown.
The app-test helper already fixes its own search path; these tests bypass it.

The wrapper now includes both the testing source gradients and the writable
${gimp_dir}/gradients token in gradient-path, using the platform path separator
and GIMP-compatible string escaping. Source data is not made writable. Explicit
user/system config overrides and non-testing behavior are preserved.

All 7 Python tests pass, including both modes, seven explicit-config forms and
quote/backslash/control-character/UTF-8 cases. All 13 affected registered native
tests pass on authenticated portrait TCP Xvfb. Their logs contain none of the
previous Failed to save data/writable-folder diagnostics, nor CRITICAL or GEGL
warnings. The report records exact changed-source hashes and all target results.
Original baseline errors remain in the earlier aggregate report. This closes
only the test fixture defect; whole-app acceptance and other diagnostics remain.
