#!/usr/bin/env python3
"""Nine XCF IO/loader/compat/test units; no RTTI/vptr, core/preservation uninstrumented."""
from pathlib import Path
source=Path(__file__).with_name('run_painter_mode_sanitizers.py').read_text()
source=source.replace('layer-mode-sanitizers','red-eye-xcf-sanitizers').replace('painter-layer-modes','painter-xcf-open')
start=source.index('wanted = {');end=source.index('\nreplacements =',start)
source=source[:start]+'''wanted = {'app/xcf/xcf.c','app/xcf/xcf-load.c','app/xcf/xcf-save.c','app/xcf/xcf-read.c','app/xcf/xcf-write.c','app/xcf/xcf-seek.c','app/tests/test-painter-xcf-open.c','app/xcf/painter-xcf-load.cpp','app/xcf/painter-xcf-compat.cpp'}
sealed = wanted | {'app/tests/test-painter-xcf-red-eye.inc','app/core/gimpfilterlayer.cpp','app/core/gimpfilterlayer.h','app/xcf/painter-xcf-preserve.cpp','app/xcf/painter-xcf-arguments.cpp'}
before = {p:hashlib.sha256((root/p).read_bytes()).hexdigest() for p in sealed}
''' +source[end:]
a=source.index('for archive in [');b=source.index(':\n',a)
source=source[:a]+'for archive in ["app/xcf/libappxcf.a"]'+source[b:]
source=source.replace('Painter legacy operation, cached direct-mode dispatch and C app test; remaining upstream GIMP/GEGL/dependencies uninstrumented','Nine IO/loader/compat/test units; vptr explicitly excluded and existing no-RTTI preserved; Filter/capsule/dependencies uninstrumented')
source=source.replace('report["source_sha256"] = {p: hashlib.sha256((root/p).read_bytes()).hexdigest() for p in wanted}', 'report["source_sha256"] = before\nreport["changed_after_run"] = [p for p,h in before.items() if hashlib.sha256((root/p).read_bytes()).hexdigest()!=h]\nif report["changed_after_run"]: raise RuntimeError("source drift")')
source=source.replace('cleaned += ["-frtti"] # consistent shared_ptr COMDAT RTTI for UBSan vptr', 'assert "-fno-rtti" in cleaned; cleaned += ["-fno-sanitize=vptr"] # preserve native RTTI ABI')
source=source.replace('flags = ["-fsanitize=address,undefined",', 'flags = ["-fsanitize=address,undefined", "-fno-sanitize=vptr",')
source=source.replace('if relative not in wanted:', 'if relative not in wanted or (relative.startswith("app/xcf/") and not entry["output"].startswith("app/xcf/libappxcf.a.p/")):')
exec(compile(source,str(Path(__file__).with_name('run_painter_mode_sanitizers.py')),'exec'))
