#!/usr/bin/env python3
"""Nine selected Color Exchange/owner/transaction units; other providers uninstrumented."""
from pathlib import Path
source=Path(__file__).with_name('run_painter_mode_sanitizers.py').read_text()
source=source.replace('layer-mode-sanitizers','color-exchange-native-sanitizers').replace('painter-layer-modes','gimp-filter-layer')
a=source.index('wanted = {');b=source.index('\nreplacements =',a)
source=source[:a]+'''wanted = {"app/core/gimpfilterlayer.cpp","app/core/gimpfiltercontext.cpp","app/painter/filter-context.cpp","app/painter/filter-native-kernels.cpp","app/painter/filter-point-shadow.cpp","app/painter/filter-raster.cpp","app/painter/filter-scheduler.cpp","app/painter/binding-store.cpp","app/tests/test-gimp-filter-layer.c"}
sealed = wanted | {"app/tests/test-filter-color-exchange-contract.inc","app/painter/filter-color-exchange.hpp","app/painter/filter-color-exchange-private.hpp","app/painter/filter-point-shadow.hpp","app/core/gimpfiltercontext.hpp","app/painter/filter-context.hpp"}
before = {p:hashlib.sha256((root/p).read_bytes()).hexdigest() for p in sealed}
'''+source[b:]
a=source.index('for archive in [');b=source.index(':\n',a)
source=source[:a]+'for archive in ["app/core/libappcore.a","app/painter/libapppainter.a"]'+source[b:]
source=source.replace('"-frtti"','"-fno-rtti", "-fno-sanitize=vptr"')
source=source.replace('Painter legacy operation, cached direct-mode dispatch and C app test; remaining upstream GIMP/GEGL/dependencies uninstrumented','Nine Color Exchange raw/shadow/native-owner, context, raster, scheduler, BindingStore and fixture units; remaining GIMP/GEGL/dependencies uninstrumented')
source=source.replace('report["source_sha256"] = {p: hashlib.sha256((root/p).read_bytes()).hexdigest() for p in wanted}','report["source_sha256"] = before\nreport["changed_after_run"] = [p for p,h in before.items() if hashlib.sha256((root/p).read_bytes()).hexdigest()!=h]\nif report["changed_after_run"]: raise RuntimeError("source drift")')
exec(compile(source,str(Path(__file__).with_name('run_painter_mode_sanitizers.py')),'exec'))
