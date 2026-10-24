#!/usr/bin/env python3
"""Bounded C XCF IO plus unknown-only fixture; C++ preservation/core uninstrumented."""
from pathlib import Path
source=Path(__file__).with_name('run_painter_mode_sanitizers.py').read_text()
source=source.replace('layer-mode-sanitizers','filter-unknown-xcf-sanitizers').replace('painter-layer-modes','painter-xcf-roundtrip')
start=source.index('wanted = {');end=source.index('\nreplacements =',start)
source=source[:start]+'''wanted = {'app/xcf/xcf.c','app/xcf/xcf-load.c','app/xcf/xcf-save.c','app/xcf/xcf-read.c','app/xcf/xcf-write.c','app/xcf/xcf-seek.c','app/tests/test-painter-xcf-roundtrip.c'}
sealed = wanted | {'app/tests/test-filter-unknown-xcf.inc','app/core/gimpfilterlayer.cpp','app/core/gimpfilterlayer.h','app/xcf/painter-xcf-preserve.cpp','app/xcf/painter-xcf-arguments.cpp'}
before = {p:hashlib.sha256((root/p).read_bytes()).hexdigest() for p in sealed}
''' +source[end:]
a=source.index('for archive in [');b=source.index(':\n',a)
source=source[:a]+'for archive in ["app/xcf/libappxcf.a"]'+source[b:]
source=source.replace('Painter legacy operation, cached direct-mode dispatch and C app test; remaining upstream GIMP/GEGL/dependencies uninstrumented','Seven C XCF IO/registration/test units; C++ Filter, capsule/provenance, shared adapters and remaining dependencies uninstrumented')
source=source.replace('report["source_sha256"] = {p: hashlib.sha256((root/p).read_bytes()).hexdigest() for p in wanted}', 'report["source_sha256"] = before\nreport["changed_after_run"] = [p for p,h in before.items() if hashlib.sha256((root/p).read_bytes()).hexdigest()!=h]\nif report["changed_after_run"]: raise RuntimeError("source drift")')
exec(compile(source,str(Path(__file__).with_name('run_painter_mode_sanitizers.py')),'exec'))
