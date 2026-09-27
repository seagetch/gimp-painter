# Legacy build requirements and current block

The legacy reference is commit `afa43fae3e920210146abed514f136fd49f671b5`
from `https://github.com/seagetch/gimp-painter.git`, `gimp-2-8`. Its
`configure.ac` declares C++14, babl >=0.1.12, GEGL 0.3 >=0.3.0, GLib
>=2.30.2, GTK 2 >=2.24.10, and json-glib >=1.0; additional optional
dependencies are listed in that file. Source location is the pinned commit
above. These are declared minima, not an exact reproducible binary lock.

The current container has no pinned GTK 2 / GEGL 0.3 development stack.
Creating an isolated old environment and recording exact package revisions
remains WBS 02.001 BLOCKED. No legacy execution evidence or compatibility
fixture is being claimed. Candidate recovery is an isolated image with
versioned build dependencies; the required measured behavior still needs to
be collected before compatibility tasks can be signed off.
