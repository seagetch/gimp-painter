#!/usr/bin/env python3
"""Build only the new trace harness against existing native archives.
Hold the shared build lock. Does not rebuild production sources.
"""
import json, shlex, subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[2];build=root/'build-debian13';out=build/'brush-geometry-probe';out.mkdir(exist_ok=True)
entry=next(e for e in json.loads((build/'compile_commands.json').read_text()) if e['file'].endswith('/painter-paper-trace.cpp'))
cmd=shlex.split(entry['command']);cmd[cmd.index('-o')+1]=str(out/'trace.o');cmd[-1]=str(root/'app/tests/painter-brush-geometry-trace.cpp')
subprocess.run(cmd,cwd=build,check=True)
link=shlex.split(subprocess.check_output(['ninja','-t','commands','app/tests/painter-paper-trace'],cwd=build,text=True).strip().splitlines()[-1]);link[link.index('-o')+1]=str(out/'trace');link=[str(out/'trace.o') if a==entry['output'] else a for a in link]
subprocess.run(link,cwd=build,check=True)
