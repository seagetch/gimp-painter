#!/usr/bin/env python3
"""Instrument float/byte kernels, native dispatch/group and C test; not all GIMP."""
from pathlib import Path
source = Path(__file__).with_name('run_painter_mode_sanitizers.py').read_text()
source = source.replace('layer-mode-sanitizers', 'layer-mode-float-sanitizers')
source = source.replace('Painter legacy operation, cached direct-mode dispatch and C app test; remaining upstream GIMP/GEGL/dependencies uninstrumented',
 'Painter byte operation, float operation/kernel, cached dispatch, GimpGroupLayer and C app test; remaining GIMP/GEGL/XCF/C++ dependencies uninstrumented')
source = source.replace('wanted = {', 'wanted = {"app/core/gimpgrouplayer.c", "app/operations/layer-modes-legacy/gimpoperationpainterfloat.c", "app/operations/layer-modes-legacy/gimppainterfloatkernel.c", ')
source = source.replace('for archive in [', 'for archive in ["app/core/libappcore.a", ')
exec(compile(source, str(Path(__file__).with_name('run_painter_mode_sanitizers.py')), 'exec'))
