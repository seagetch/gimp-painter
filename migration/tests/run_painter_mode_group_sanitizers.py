#!/usr/bin/env python3
"""Run the existing focused mode sanitizer with group source also instrumented.

This wrapper derives the existing runner with explicit target substitutions.
Original build objects are not replaced. Hold the shared native build lock.
"""
from pathlib import Path
import sys
source = Path(__file__).with_name('run_painter_mode_sanitizers.py').read_text()
source = source.replace('layer-mode-sanitizers', 'layer-mode-group-sanitizers')
source = source.replace('Painter legacy operation, cached direct-mode dispatch and C app test; remaining upstream GIMP/GEGL/dependencies uninstrumented',
                        'Painter legacy operation, cached dispatch, GimpGroupLayer and C app test; remaining GIMP/GEGL/dependencies uninstrumented')
source = source.replace('wanted = {', 'wanted = {"app/core/gimpgrouplayer.c", ')
source = source.replace('for archive in [', 'for archive in ["app/core/libappcore.a", ')
exec(compile(source, str(Path(__file__).with_name('run_painter_mode_sanitizers.py')), 'exec'))
