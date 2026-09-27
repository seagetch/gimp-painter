# GIMP 3.0 branch build definition

At upstream commit `95f6410f25c5186686db7a489d79c1e79187cd41`, the root
`meson.build` project declaration enables both `c` and `cpp`, sets
`cpp_std=c++14`, and requires Meson `>=0.61.0`. Its version string is `3.0.9`;
that string does not assert a published 3.0.9 release. This is the unmodified
destination definition for WBS 03.001; actual platform compilation remains
unverified.
