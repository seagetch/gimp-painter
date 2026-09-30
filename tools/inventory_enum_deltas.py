#!/usr/bin/env python3
"""Compare enum members/brush constant values against the fixed legacy base."""
import ast,csv,re,subprocess
from pathlib import Path
from inventory_call_boundaries import without_comments
ROOT=Path('migration/inventory')
ENUM=re.compile(r'\benum\s*(?:class\s+)?(?:\w+\s*)?\{([^{}]*)\}\s*(\w*)',re.S)
def number(expr,names):
 def walk(n):
  if isinstance(n,ast.Constant) and isinstance(n.value,int):return n.value
  if isinstance(n,ast.Name):return names[n.id]
  if isinstance(n,ast.UnaryOp):return -walk(n.operand) if isinstance(n.op,ast.USub) else walk(n.operand) if isinstance(n.op,ast.UAdd) else ~walk(n.operand)
  if isinstance(n,ast.BinOp):
   a,b=walk(n.left),walk(n.right)
   ops={ast.Add:lambda:a+b,ast.Sub:lambda:a-b,ast.LShift:lambda:a<<b,ast.BitOr:lambda:a|b,ast.BitAnd:lambda:a&b,ast.Mult:lambda:a*b}
   return ops[type(n.op)]()
  raise ValueError(expr)
 try:return walk(ast.parse(expr,mode='eval').body)
 except (KeyError,ValueError,SyntaxError,TypeError):return expr

def parse(raw):
 code=without_comments(raw);result={};names={}
 for m in ENUM.finditer(code):
  previous=-1
  for member in m[1].split(','):
   x=re.fullmatch(r'\s*(\w+)\s*(?:=\s*(.+?))?\s*',member,re.S)
   if not x:continue
   value=number(x[2],names) if x[2] else previous+1 if isinstance(previous,int) else f'({previous})+1'
   name=x[1];previous=value;names[name]=value
   offset=m.start(1)+m[1].find(member)+x.start(1);result[name]=(str(value),code.count('\n',0,offset)+1,m[2] or 'anonymous/internal')
 for m in re.finditer(r'^\s*#define\s+((?:BRUSH|INPUT|STATE)_\w+)\s+([^\n]+)',code,re.M):
  value=number(m[2].strip(),names);names[m[1]]=value;result[m[1]]=(str(value),code.count('\n',0,m.start(1))+1,'generated-brush-constant')
 return result

def route(path):
 if path=='app/core/mypaintbrush-enum-settings.h':return ('generate.py absent in legacy tree; reconstruct from brushsettings.c and ordered constants','app/core/mypaintbrush-enum-settings.h;app/core/mypaintbrush-brushsettings.c','myb setting/input names; numeric index is internal, not file key','04.013/brush-setting-generator,19.003')
 if path=='app/xcf/xcf-private.h':return ('handwritten; no enum generator','none','XCF numeric wire IDs','10.004,10.006')
 if path.endswith('-enums.h') or path=='libgimpbase/gimpbaseenums.h':
  c=path[:-1]+'c'; return ('tools/gimp-mkenums via directory Makefile.am; public PDB enumgen.pl/enumcode.pl',c+';tools/pdbgen/enums.pl','mode: XCF numeric; dynamics/context: config nick or flags; remaining internal enum values','04.013,10.005,30.009')
 if path=='libgimp/gimpenums.h':return ('tools/pdbgen/enumgen.pl + enumcode.pl','libgimp/gimpenums.c;tools/pdbgen/enums.pl','public PDB/XCF mode numbers must translate','04.013,10.005')
 return ('handwritten local enum; no build generator','none','internal property/state IDs; config persists property name, not ordinal','04.013,30.009')

def main():
 with (ROOT/'changed-files.tsv').open() as f:files=list(csv.DictReader(f,delimiter='\t'))
 rows=[]
 for info in files:
  path=info['path']
  if not path.endswith(('.c','.h','.cpp','.hpp')) or not info['source_git_blob']:continue
  raw=subprocess.check_output(['git','show',info['source_git_blob']],text=True,errors='replace')
  if not re.search(r'\benum\b|#define\s+(?:BRUSH|INPUT|STATE)_',raw):continue
  old=subprocess.check_output(['git','show',info['base_git_blob']],text=True,errors='replace') if info['base_git_blob'] else ''
  before=parse(old);after=parse(raw)
  for name,(value,line,enum) in after.items():
   if name in before and before[name][0]==value:continue
   rows.append((f'{path}:{line}',enum,name,before.get(name,('-',))[0],value,*route(path),'STATIC_DELTA'))
 with (ROOT/'enum-deltas.tsv').open('w') as f:
  w=csv.writer(f,delimiter='\t',lineterminator='\n');w.writerow(('legacy_site','enum_or_group','name','base_value','legacy_value','generator','outputs','storage_contract','followup','status'));w.writerows(rows)
 print(f'{len(rows)} added/renumbered enum members and brush constants in {len({r[0].split(":")[0] for r in rows})} files')
if __name__=='__main__':main()
