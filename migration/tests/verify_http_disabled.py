#!/usr/bin/env python3
"""Verify that a disabled build contains no optional HTTP sources or Soup linkage."""
import argparse,json,pathlib,subprocess
p=argparse.ArgumentParser();p.add_argument('build',type=pathlib.Path);p.add_argument('--report',type=pathlib.Path,required=True);a=p.parse_args();b=a.build.resolve()
commands=json.loads((b/'compile_commands.json').read_text())
http_sources=[entry['file'] for entry in commands if '/httpd/' in entry['file']]
config=(b/'config.h').read_text()
executables={}
for name in ['gimp-3.0','gimp-console-3.0']:
    path=b/'app'/name
    dynamic=subprocess.check_output(['readelf','-d',str(path)],text=True)
    symbols=subprocess.check_output(['nm',str(path)],text=True)
    executables[name]={'soup_needed':'libsoup' in dynamic,'http_start_symbol':'gimp_painter_httpd_start' in symbols}
assert not http_sources,http_sources
assert '#define HAVE_PAINTER_HTTP' not in config
assert all(not v['soup_needed'] and not v['http_start_symbol'] for v in executables.values())
a.report.write_text(json.dumps({'http_sources':http_sources,'http_config_defined':False,'executables':executables,'status':'PASS'},indent=2)+'\n')
print('HTTP-disabled compile/link boundary PASS')
