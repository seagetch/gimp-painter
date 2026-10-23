#!/usr/bin/env python3
"""Focused C XCF plus mode/group/test instrumentation, no C++ capsule claim.

Original production objects are not replaced. Hold the shared native build lock.
"""
from pathlib import Path
source = Path(__file__).with_name('run_painter_mode_sanitizers.py').read_text()
source = source.replace('layer-mode-sanitizers', 'layer-mode-xcf-sanitizers')
source = source.replace('Painter legacy operation, cached direct-mode dispatch and C app test; remaining upstream GIMP/GEGL/dependencies uninstrumented',
                        'XCF C load/save/read/write/seek/registration, Painter byte operation, cached dispatch, GimpGroupLayer and C test; C++ capsule/provenance and remaining dependencies uninstrumented')
source = source.replace('wanted = {', '''wanted = {"app/core/gimpgrouplayer.c",
 "app/xcf/xcf.c", "app/xcf/xcf-load.c", "app/xcf/xcf-save.c",
 "app/xcf/xcf-read.c", "app/xcf/xcf-write.c", "app/xcf/xcf-seek.c", ''')
source = source.replace('for archive in [', 'for archive in ["app/core/libappcore.a", "app/xcf/libappxcf.a", ')
source = source.replace('report.update({"exit_code":', 'report["test_include_sha256"] = hashlib.sha256((root/"app/tests/test-painter-mode-xcf.inc").read_bytes()).hexdigest()\nreport.update({"exit_code":')
exec(compile(source, str(Path(__file__).with_name('run_painter_mode_sanitizers.py')), 'exec'))
