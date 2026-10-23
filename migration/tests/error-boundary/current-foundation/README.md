# Current foundation verification, separate from original05.013

The original report.json and native-results.json are unchanged. Their source hashes
match pinned commit f299d8a5f3982e8bfcae366dc0d2e5a573f6090b in historical-source.tar.gz.
The historical successor TODO assertions are verified against that snapshot;
current successor rows may complete independently and retain05.013 dependencies.

Fresh standalone C11/C++14 foundation tests pass71/71 in normal and ASan+UBSan
builds, including6 interop,2 boundary,12 store tests. This is real native GLib/C++
code, not full GIMP integration or old-layout ABI compatibility. LSan is disabled;
-fno-rtti means no RTTI-dependent vptr claim. Every source hash is current and
unchanged across compile/test. Builds and HOME/XDG/TMPDIR were private under an
isolated directory; no shared Meson outputs, native UI profile, or build lock were
mutated. This independent short build did not jump native UI acceptance queues.

`python3 tools/check_painter_error_boundary.py` verifies both distinctly. New
foundation source changes must supply new current results; old receipts are never
resealed as current. Successor completion remains each task's separate obligation.
