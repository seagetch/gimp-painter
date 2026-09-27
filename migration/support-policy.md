# Initial port support policy

- Implementation and baseline verification target the upstream `gimp-3-0`
  branch, pinned in `baseline/baseline.json`. The published `GIMP_3_0_8` tag
  is a separate, fixed comparison reference.
- Additional 3.x series, including `gimp-3-2`, require their own API audit,
  build, legacy document round trip, drawing comparison, and platform tests
  before a claim of support. Branch name alone grants no compatibility.
- Initial validation targets Linux x86_64, Windows x86_64, and macOS arm64 and
  x86_64. No target is marked supported until its package and required tests
  have passed on that architecture.
- C++14 is the initial standard. Keep existing GIMP C translation units in C;
  new C++ implementation enters through narrow C and GObject boundaries.
