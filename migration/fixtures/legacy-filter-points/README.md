# Genuine original point-filter outputs

Forty output buffers were captured by `migration/tests/capture_filter_points.py`
from the pinned gimp-painter commit afa43fae3e920210146abed514f136fd49f671b5.
The actual installed 2.8 executable, plug-in binaries, source files, generated
Script-Fu command, output log and buffers are hashed in capture-report.json.
No replacement kernel, XCF reader or modern GEGL operation generated the oracle.

The corpus includes Value Invert, signed Minimum/Maximum RGB flags and Threshold
Alpha at -1, 0, 127, 255 and 256, using RGB, RGBA and Gray-alpha native inputs, zero/low
alpha, a single pixel and a 67×66 tile-boundary image. Five input load/export checks
preserve all bytes. Threshold Alpha is callable directly in old FilterLayer
specifications although its Layer/Transparency menu is excluded by the popup.

Reproduction uses the already established isolated legacy environment and a
fresh output directory (existing capture-report.json is never overwritten):

```
source /workspace/shared/gimp-legacy-build/env.sh
python3 migration/tests/capture_filter_points.py --capture --output /path/to/new/corpus
```

Hold the shared build/test lock for capture. Only generated test images are used.
The runtime profile/pluginrc and inherited environment are not evidence artifacts.
These are whole, unselected drawable outputs, not all lower-stack composition,
indexed palette behavior, high-precision output or arbitrary legacy PDB support.
