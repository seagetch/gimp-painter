#!/usr/bin/env python3
"""Native controls for startup-root inspection, using real GObject and GTK."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-dir', type=Path, required=True)
    args = parser.parse_args()
    out = args.output_dir.resolve(); out.mkdir(parents=True,exist_ok=True)
    spec = importlib.util.spec_from_file_location('checker', ROOT/'tools/check_cpp_static_initialization.py')
    checker = importlib.util.module_from_spec(spec); spec.loader.exec_module(checker)
    commands, cases = [], []
    def run(argv, name):
        argv = list(map(str,argv))
        result = subprocess.run(argv,capture_output=True,text=True)
        commands.append({'name':name,'argv':argv,'exit_code':result.returncode,
                         'stdout':result.stdout,'stderr':result.stderr})
        if result.returncode:
            raise RuntimeError(name+': '+result.stderr)
        return result
    cxx = shlex.split(os.environ.get('CXX','c++'))
    cflags = shlex.split(run(['pkg-config','--cflags','gtk+-3.0'],'cflags').stdout)
    libs = shlex.split(run(['pkg-config','--libs','gtk+-3.0'],'libs').stdout)
    common = r'''
#include <gtk/gtk.h>
#include <cassert>
static bool entered_main;
static unsigned early_calls, all_calls;
static GType register_probe() {
  if (!entered_main) ++early_calls;
  ++all_calls;
  GType type = g_type_register_static_simple(G_TYPE_OBJECT, "PainterOrderControl",
      sizeof(GObjectClass), nullptr, sizeof(GObject), nullptr, GTypeFlags(0));
  assert(type && gtk_window_get_type());
  return type;
}
'''
    programs = {
        'lazy-safe': common+r'''
static GType type() { static const GType value = register_probe(); return value; }
int main() { assert(!early_calls && !all_calls); entered_main=true;
  assert(type() && type()); assert(!early_calls && all_calls==1); }
''',
        'global-type': common+r'''
static GType value = register_probe();
int main() { assert(value && early_calls==1 && all_calls==1); entered_main=true; }
''',
        'arbitrary-constructor': common+r'''
__attribute__((constructor)) static void any_name() { register_probe(); }
int main() { assert(early_calls==1 && all_calls==1); entered_main=true; }
''',
        'priority-constructor': common+r'''
__attribute__((constructor(200))) static void another_name() { register_probe(); }
int main() { assert(early_calls==1 && all_calls==1); entered_main=true; }
''',
        'constant-only': r'''
#include <atomic>
static std::atomic<unsigned> count {0};
static const char *const name = "constant";
int main() { return count.load()!=0 || name[0]!='c'; }
''',
        'ifunc-resolver': r'''
extern "C" int implementation() { return 0; }
extern "C" auto resolve_entry() -> int (*)() { return implementation; }
extern "C" int entry() __attribute__((ifunc("resolve_entry")));
int main() { return entry(); }
''',
        'tls-initializer': common+r'''
thread_local GType per_thread_type = register_probe();
int main() { entered_main=true; assert(per_thread_type); }
''',
    }
    safe = {'constant-only','lazy-safe'}
    for name, source in programs.items():
        path=out/(name+'.cpp'); path.write_text(source)
        obj=out/(name+'.o')
        run(cxx+['-std=c++14','-fno-lto','-c',path,'-o',obj]+cflags,name+'-compile')
        actual=checker.inspect_object(obj)
        rejected=bool(actual['findings'])
        assert rejected == (name not in safe), (name,actual)
        executable=out/name
        run(cxx+[obj,'-o',executable]+libs,name+'-link')
        # IFUNC/TLS controls exercise inspection only: a TLS initializer can be
        # deferred safely and needs separate review, not a pre-main assertion.
        executed=name not in ('ifunc-resolver','tls-initializer')
        if executed: run([executable],name+'-run')
        cases.append({'name':name,'expected_rejected':name not in safe,
                      'observed_rejected':rejected,'executed':executed,
                      'inspection':actual})
    # The strict non-LTO/native precondition must fail explicitly.
    lto=out/'lto.o'
    run(cxx+['-std=c++14','-flto','-c',out/'constant-only.cpp','-o',lto],'lto-compile')
    try: checker.inspect_object(lto)
    except ValueError as error:
        assert 'LTO object' in str(error)
        cases.append({'name':'lto-rejected','expected_rejected':True,'observed_rejected':True})
    else: raise AssertionError('LTO input was silently accepted')
    (out/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
    (out/'report.json').write_text(json.dumps({'status':'PASS','cases':cases,
        'scope':'Real GType/GTK startup controls; no display or GUI widget instance created'},indent=2)+'\n')
    print('PASS: '+str(len(cases))+' startup-root controls; real early/lazy GType/GTK observations')


if __name__ == '__main__':
    main()
