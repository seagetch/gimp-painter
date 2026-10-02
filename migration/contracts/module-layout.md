# Painter module layout (04.001)

The pinned destination already enables C and C++14. Existing C files remain C;
this port adds C++ modules to the same Meson build and executable.

## Dependency direction

1. `app/painter`: shared ownership, BindingStore, exception boundaries,
   connection/source management and small lifetime tests. GLib/GObject only;
   no GTK, GEGL, JSON, PDB or feature headers. One library, one store mechanism.
2. `app/core`: C-compatible GimpLayer/data/Undo/model type adapters and their
   private C++ state. C operation declarations stay in `.h`; named C++ handles
   and implementation details stay in `.hpp`.
3. `app/paint`: brush evaluation/Surface state and pixel algorithms. Pure
   algorithms remain independent of UI; buffer adapters consume current GEGL
   formats and Undo. Generated brush constants belong with this module.
4. `app/pdb`: execution adapters and owned argument values; worker input/results
   are independent of GUI owners. The common store is never the executor.
5. `app/display`, `app/tools`, `app/widgets`: GTK3/view/tool adapters and declared
   behavior slots. They call existing C operations through named handles and
   disconnect before owner destruction. They do not own a second binding system.
6. `app/presets`: JSON resource/application logic, depending explicitly on
   JSON-GLib and core operations. Reader/writer direct linkage will be verified
   with the actual feature module; the common foundation does not absorb JSON.
7. Optional HTTP remains outside mandatory core dependencies. Core must not
   import HTTP headers or require its server to start.

The app compiles `libapppainter` as a private static C++14 library and links it
into GUI, console and app test links. `Gimp.dispose` invokes its no-op-if-unbound
C close hook before resource/context teardown, ensuring the real app references
and retains the bridge object. No dummy source or C++ static constructor is used
to force linkage. Feature get_type/registration calls must be explicit from their
own initialization paths, never side effects of archive member discovery.

## Interface and ownership rules

`gimp-painter-error.h` and `gimp-painter-binding.h` are C-compatible internal app
headers with include guards and G_BEGIN_DECLS/G_END_DECLS. They expose no C++
types. No common C++ header is installed. C entry, vfunc and signal adapters all
resolve the same compile-time slot; no RTTI/private-placement/free-key state
lookup is introduced. The detailed contract is `cpp-foundation.md`.

The module test executable has C-compiled main and GObject subclass adapters,
C++-compiled implementation, a static bridge archive and explicit C++ final link.
The final GIMP link is explicitly C++ without changing source-language detection.
All target-specific exception/RTTI/visibility settings are local to the bridge.

## Scope of evidence

Successful foundation compilation does not establish that custom GIMP types,
old files, brush behavior or UI have been migrated. The legacy type registry and
hunk/asset work ledger keep their implementation states open. Feature-specific
headers, generated definitions, direct JSON linkage and platform tests are
checked as their modules are introduced, not inferred from this layout.
