#!/usr/bin/env python3
"""Extend focused native Filter proof to Colorify owner-context and shadow units."""
from pathlib import Path
source=Path(__file__).with_name('run_filter_layer_sanitizers.py').read_text()
source=source.replace('wanted = {','wanted = {"app/core/gimpfiltercontext.cpp", "app/painter/filter-context.cpp", "app/painter/filter-point-shadow.cpp", ')
source=source.replace('"app/core/gimpfilterlayer.h",','"app/tests/test-filter-colorify-contract.inc", "app/painter/filter-colorify-kernel.hpp", "app/painter/filter-point-shadow.hpp", "app/core/gimpfiltercontext.hpp", "app/painter/filter-context.hpp", "app/core/gimpfilterlayer.h",')
source=source.replace('FilterLayer, argument/Undo state','Colorify raw/shadow/owner-context plus FilterLayer, argument/Undo state')
exec(compile(source,str(Path(__file__).with_name('run_filter_layer_sanitizers.py')),'exec'))
