#!/usr/bin/env python3
"""Source-derived metadata contract for 10.001 configuration/preset routes.

This is a static schema/default/flags assertion, not a runtime roundtrip test.
Both revisions are parsed independently. Expected fields are derived from
registrations and explicit reader/writer schemas, never ledger row counts.
"""
import argparse, csv, hashlib, json, re, subprocess
from pathlib import Path
from functools import lru_cache
LEGACY = 'afa43fae3e920210146abed514f136fd49f671b5'
TEST_FILE = 'migration/tests/test_persistence_config_contract.py'
MANUAL = 'migration/inventory/persistence-config-manual-schema.json'


def norm(s):
    return re.sub(r'\s+', ' ', s).strip()


def strip_comments(s):
    return re.sub(r'("(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\')|/\*.*?\*/|//[^\n]*',
                  lambda m: m.group(1) or ' ' * len(m.group()), s, flags=re.S)


def call_args(s, start):
    """Balanced C expression arguments, preserving nested calls and strings."""
    p = s.index('(', start); level = 1; pos = p + 1; begin = pos; args = []
    quote = None
    while pos < len(s):
        c = s[pos]
        if quote:
            if c == '\\': pos += 2; continue
            if c == quote: quote = None
        elif c in '\"\'': quote = c
        elif c in '([{': level += 1
        elif c in ')]}':
            level -= 1
            if not level:
                args.append(s[begin:pos].strip()); return args, pos + 1
        elif c == ',' and level == 1:
            args.append(s[begin:pos].strip()); begin = pos + 1
        pos += 1
    raise AssertionError('unclosed call')


class Source:
    def __init__(self, root): self.root, self.cache = Path(root), {}
    def read(self, path, revision='native'):
        k = (path, revision)
        if k not in self.cache:
            if revision == 'native': self.cache[k] = (self.root / path).read_text()
            else: self.cache[k] = subprocess.check_output(['git', 'show', LEGACY + ':' + path], cwd=self.root, text=True)
        return self.cache[k]
    @lru_cache(None)
    def files(self, rev):
        if rev == 'native':
            return subprocess.check_output(['git', 'ls-files'], cwd=self.root, text=True).splitlines()
        return subprocess.check_output(['git', 'ls-tree', '-r', '--name-only', LEGACY], cwd=self.root, text=True).splitlines()


def source_routes(src, rev):
    # Source selectors deliberately do not read the contract. Includes all
    # standard option subclasses/interfaces, and nested aggregate schemas.
    result = {}
    for path in src.files(rev):
        if re.fullmatch(r'app/config/gimp(?:base|gegl|core|display|gui|dialog|plugin)config.c', path) or path == 'app/config/gimpdisplayoptions.c': result[path] = '11'
        if re.fullmatch(r'app/(?:paint|tools)/gimp\w*options.c', path) and 'mypaint' not in path : result[path] = '12'
    result.update({'app/paint/gimpfillbrush.cpp':'12','app/paint/gimppaintersmudge.cpp':'12','app/core/gimptooloptions.c':'12', 'app/core/gimpcontext.c':'13',
                   'app/core/gimptemplate.c':'11', 'app/core/gimpgrid.c':'11',
                   'libgimpconfig/gimpcolorconfig.c':'11', 'app/core/gimpfilloptions.c':'11',
                   'app/core/gimpstrokeoptions.c':'11','app/core/gimptoolpreset.c':'16',
                   'app/core/gimpdynamics.c':'16','app/core/gimpdynamicsoutput.c':'16',
                   'app/core/gimpcurve.c':'16','app/core/gimpviewable.c':'16',
                   'app/core/gimptoolitem.c':'15','app/core/gimptoolgroup.c':'15'})
    available = set(src.files(rev))
    return {p:r for p,r in result.items() if p in available}


def function_body(text,name):
    clean=strip_comments(text)
    for found in re.finditer(r'\b'+re.escape(name)+r'\s*\(',clean):
        _,end=call_args(clean,found.start())
        while end<len(clean) and clean[end].isspace():end+=1
        if end>=len(clean) or clean[end]!='{':continue
        start=end;depth=1;end+=1;quote=None
        while depth and end<len(clean):
            c=clean[end]
            if quote:
                if c=='\\':end+=2;continue
                if c==quote:quote=None
            elif c in '"\'':quote=c
            elif c=='{':depth+=1
            elif c=='}':depth-=1
            end+=1
        return norm(clean[start:end])
    raise AssertionError(('function definition missing',name))


def source_guard(text):
    stack=[]
    for line in text.splitlines():
        m=re.match(r'\s*#\s*(if|ifdef|ifndef|elif|else|endif)\b(.*)',line)
        if not m:continue
        op,arg=m.groups()
        if op in ('if','ifdef','ifndef'):stack.append(op+' '+arg.strip())
        elif op in ('else','elif') and stack:stack[-1]=op+' ('+stack[-1]+') '+arg.strip()
        elif op=='endif' and stack:stack.pop()
    return ' && '.join(stack) or 'unconditional'


def preprocessor_definitions(text):
    text=re.sub(r'\\\n', ' ',text)
    defs={};stack=[]
    for line in text.splitlines():
        m=re.match(r'\s*#\s*(if|ifdef|ifndef|elif|else|endif)\b(.*)',line)
        if m:
            op,arg=m.groups()
            if op in ('if','ifdef','ifndef'):stack.append(op+' '+arg.strip())
            elif op in ('else','elif') and stack:stack[-1]=op+' ('+stack[-1]+') '+arg.strip()
            elif op=='endif' and stack:stack.pop()
        m=re.match(r'\s*#define\s+(\w+)\s+(.+)',line)
        if m:defs.setdefault(m[1],[]).append((' && '.join(stack),norm(m[2])))
    return defs


def resolve_expr(expr, before, whole, include_text=''):
    """Preserve conditional declarations and runtime initialization programs."""
    expr=norm(expr);definitions=preprocessor_definitions(include_text+'\n'+whole)
    def expand(token):
        matches=definitions.get(token)
        if not matches:return token
        unique=list(dict.fromkeys(matches))
        if len(unique)==1 and not unique[0][0]:return unique[0][1]
        return 'conditional {'+'; '.join((g or 'unconditional')+': '+v for g,v in unique)+'}'
    for _ in range(5):
        old=expr;expr=re.sub(r'\b[A-Z][A-Z0-9_]+\b',lambda m:expand(m[0]),expr)
        if expr==old:break
    var=expr.lstrip('&')
    if re.fullmatch(r'[a-z][a-z_0-9]*',var):
        assignments=list(re.finditer(r'\b'+re.escape(var)+r'\s*=\s*([^;]+);',before,re.S))
        if assignments:
            last=assignments[-1]
            if len(assignments)>1 and (re.search(r'\b'+re.escape(var)+r'\b',last[1]) or var in ('memory_size','n_threads','undo_size','mypaint_brushes')):
                expr=var+' evaluated by {'+norm(before[assignments[0].start():last.end()])+'}'
            else:expr=norm(last[1])
            mutations=re.findall(r'\b(?:gimp_\w+|gegl_\w+)\s*\(\s*&?'+re.escape(var)+r'\s*,[^;]+;',before[last.end():],re.S)
            if mutations:expr+='; then '+norm(' '.join(mutations))
        else:
            mutations=re.findall(r'\b(?:gimp_\w+|gegl_\w+)\s*\(\s*&?'+re.escape(var)+r'\s*,[^;]+;',before,re.S)
            if mutations:expr=norm(mutations[-1])
    return expr


def include_defaults(src,path,rev,raw):
    available=set(src.files(rev));parts=[]
    for header in re.findall(r'^#include "([^"]+)"',raw,re.M):
        if header not in ('gimptemplate.h','base/siox.h','config-types.h','gimpguiconfig.h','gimpdisplayconfig.h'): continue
        for candidate in (str(Path(path).parent/header),'app/'+header,header):
            if candidate in available:
                parts.append(strip_comments(src.read(candidate,rev)));break
    return '\n'.join(parts)


def registrations(src, path, rev):
    raw = src.read(path, rev); s = strip_comments(raw); header_defaults=include_defaults(src,path,rev,raw)
    defs = list(re.finditer(r'(\w+)_(?:class|default)_init\s*\(\s*(\w+?)(?:Class|Interface)\s*\*[^)]*\)\s*\{', s))
    types = list(re.finditer(r'G_DEFINE_(?:ABSTRACT_)?TYPE\w*\s*\(\s*(\w+)', s))
    result = []
    regex = r'\b(GIMP_CONFIG_(?:INSTALL_)?PROP_[A-Z0-9_]+|g(?:imp)?_param_spec_[a-z_0-9]+)\s*\('
    occupied = []
    for m in re.finditer(regex, s):
        if any(a <= m.start() < b for a,b in occupied): continue
        args,end = call_args(s,m.start()); macro = m.group(1)
        flags = norm(args[-1])
        ismacro = macro.startswith('GIMP_CONFIG_')
        if ismacro:
            occupied.append((m.start(),end))
            suffix = macro.split('PROP_',1)[1].lower()
            a = args[2:]
            if 'INSTALL_' in macro: a.insert(1,'NULL')
            header = src.read('libgimpconfig/gimpconfig-params.h',rev)
            logical = re.sub(r'\\\n',' ',header)
            body = re.search(r'^#define '+re.escape(macro)+r'\([^\n]+',logical,re.M)
            assert body, ('macro definition missing',macro,rev)
            added = 'GIMP_CONFIG_PARAM_FLAGS' if 'GIMP_CONFIG_PARAM_FLAGS' in body[0] else 'G_PARAM_READWRITE | GIMP_CONFIG_PARAM_SERIALIZE'
            assert added == 'GIMP_CONFIG_PARAM_FLAGS' or ('G_PARAM_READWRITE' in body[0] and 'GIMP_CONFIG_PARAM_SERIALIZE' in body[0])
            flags = norm(a[-1]) + ' | ' + added
        else:
            if not ('GIMP_CONFIG_PARAM_FLAGS' in flags or 'GIMP_CONFIG_PARAM_SERIALIZE' in flags): continue
            suffix = macro.split('_spec_',1)[1]; a = args
        owners = [x for x in defs if x.start() < m.start()]
        owner = owners[-1].group(2) if owners else types[0].group(1) if types else None
        if not owner:
            iface = re.search(r'\b(Gimp\w+)Interface\s*\*',s)
            owner = iface.group(1) if iface else None
        assert owner, (path,macro)
        name = norm(a[0])
        if 'gimp_context_prop_names' in name:
            symbol = re.search(r'\[GIMP_CONTEXT_PROP_(\w+)\]',name).group(1)
            name = '"' + symbol.lower().replace('_','-') + '"'
            names_array=re.search(r'gimp_context_prop_names\[\]\s*=\s*\{(.*?)\};',s,re.S)
            assert names_array and name in re.findall(r'"[^"]+"',names_array[1]), ('context name table mismatch',name,rev)
        if not re.fullmatch(r'"[^"]+"',name):
            raise AssertionError(('unresolved property name',rev,path,name))
        name = name[1:-1]
        extra = a[3:-1]
        if suffix in ('object','boxed','pointer','value_array','object_array'):
            default = 'NULL'; typ = extra[0] if extra else suffix
        else:
            default = extra[-1] if extra else 'NULL'
            typ = {'boolean':'gboolean','int':'gint','uint':'guint','int64':'gint64','uint64':'guint64',
                   'double':'gdouble','string':'gchar*','path':'gchar* (GIMP_CONFIG_PATH_DIR_LIST)',
                   'config_path':'gchar* (GIMP_CONFIG_PATH_DIR_LIST)','memsize':'guint64 bytes',
                   'resolution':'gdouble dpi','unit':'GimpUnit','rgb':'GimpRGB', 'color':'GeglColor*'}.get(suffix)
            if suffix in ('enum','flags'): typ = extra[0]
            if typ is None: typ = 'GParamSpec ' + suffix + ': ' + ', '.join(extra)
        if suffix in ('path','config_path') and extra: typ = 'gchar* (' + extra[0] + ')'
        if suffix in ('enum','flags'): typ += ' (serialized enum nick)' if suffix=='enum' else ' (serialized flag nick set)'
        if suffix == 'value_array':
            specs=list(re.finditer(r'array_spec\s*=\s*',s[:m.start()]))
            if specs:
                expr_start=specs[-1].end(); expr_end=s.index(';',expr_start)
                typ = 'GimpValueArray; element spec ' + norm(s[expr_start:expr_end])
        default = resolve_expr(default,s[:m.start()],s,header_defaults)
        if suffix in ('object','boxed','value_array'):
            field = name.replace('-', '_')
            init_assign = list(re.finditer(r'\b(?:config|private|priv|curve|options|context|tool_preset|tool_group->priv)->' + re.escape(field) + r'\s*=\s*([^;]+);', s))
            construction = [norm(x.group(1)) for x in init_assign if not re.search(r'g_value_|g_object_|g_strdup|gimp_config_duplicate', x.group(1)) or 'g_object_new' in x.group(1)]
            # Assignments in *_init/constructed are selected by their lexical
            # function interval, avoiding getters or later runtime setters.
            init_spans=[]
            for fn in re.finditer(r'\b\w+_(?:init|constructed)\s*\([^;{}]*\)\s*\{',s):
                fnstart=s.index('{',fn.start()); depth=1; fnend=fnstart+1
                while depth and fnend<len(s):
                    depth += (s[fnend]=='{')-(s[fnend]=='}'); fnend+=1
                init_spans.append((fnstart,fnend))
            construction=[norm(x.group(1)) for x in init_assign if any(a<=x.start()<b for a,b in init_spans)]
            default = 'GParamSpec NULL; instance initialization: ' + (resolve_expr('; '.join(construction),'',s,header_defaults) if construction else 'NULL (zero-initialized; no assigning constructor)')
            if owner=='GimpDialogConfig' and name in ('fill-options','stroke-options'):
                helper='gimp_'+name.replace('-','_')+'_new'
                helper_path='app/core/gimp'+name.replace('-','')+'.c'
                code=src.read(helper_path,rev)
                assert 'gimp_context_set_parent (GIMP_CONTEXT (options), context);' in code
                default += '; foreground/background/pattern inherit current user context because use_context_color=TRUE; constructed sets serialize_props=0, so those context values are not emitted inside this aggregate'
                if name=='stroke-options':
                    assert 'paint_info = gimp_paint_info_get_standard (gimp);' in code
                    default += '; paint-info uses current context paint-info, otherwise standard paint info'
            if owner=='GimpTextOptions' and name=='outline-pattern': typ += ' referenced by resource-name string or NULL token'
        if owner == 'GimpCurve':
            if name == 'samples':
                assert 'curve->samples[i] = (gdouble) i / (gdouble) (curve->n_samples - 1);' in raw
                assert 'gimp_curve_set_n_samples (curve, g_value_get_int (value));' in raw
                default = 'GParamSpec NULL; CONSTRUCT n-samples=256 allocates 256 doubles with sample[i]=i/255 for i=0..255; NULL samples setter keeps this array'
            elif name == 'points':
                if rev == 'legacy':
                    assert 'curve->points[curve->n_points - 1].x = 1.0;' in raw
                    assert 'gimp_curve_set_n_points (curve, g_value_get_int (value));' in raw
                    default = 'GParamSpec NULL; CONSTRUCT n-points=17 allocates17 pairs: (0,0), fifteen (-1,-1) unused pairs, (1,1); NULL points setter leaves them'
                else:
                    assert 'gimp_curve_clear_points (curve);' in raw
                    default = 'GParamSpec NULL; instance n_points=0; NULL points/point-types construct setters clear points; getter serializes empty GimpValueArray'
            elif name == 'point-types':
                default = 'GParamSpec NULL; instance n_points=0 and NULL construct setter clears points; getter serializes empty GimpValueArray'
        if owner == 'GimpViewable':
            if name == 'icon-pixbuf': typ += ' encoded PNG bytes as base64 quoted string'
            if name in ('icon-pixbuf','icon-name','stock-id'): flags += '; custom writer emits only non-NULL explicitly stored icon, not class display fallback'
        if owner == 'GimpContext' and suffix == 'object':
            typ += ' referenced by quoted GimpObject resource name or NULL token'
            decoder=function_body(raw,'gimp_context_deserialize_property')
            enum_name=('GIMP_CONTEXT_PROP_PAINTER_MYBRUSH_LEGACY' if name=='mypaint-brush' and rev=='native' else 'GIMP_CONTEXT_PROP_'+name.upper().replace('-','_'))
            after=decoder.split('case '+enum_name+':',1)[1]
            block=after.split('break;',1)[0]
            fallback=re.search(r'standard\s*=\s*([^;]+);',block)
            container=re.search(r'container\s*=\s*([^;]+);',block)
            if rev=='native':
                assert fallback, (path,name,rev)
                default += '; unresolved saved name selects '+fallback[1]+' from '+container[1]+' and caches requested name; NULL token clears reference'
            else:
                current=re.search(r'current\s*=\s*([^;]+);',block);assert current
                nodata='TRUE' if 'no_data = TRUE;' in block else 'context->gimp->no_data'
                default += '; unresolved saved name in '+container[1]+': when '+nodata+' cache name and set NULL, otherwise retain '+current[1]+'; explicit NULL token clears reference'
            if name == 'mypaint-brush' and rev == 'native': flags += '; read alias for painter-mybrush; excluded by default serialize_props mask'
        guard=source_guard(raw[:m.start()])
        flags += '; build condition: '+guard
        result.append(dict(name=name, owner=owner, kind=suffix, type=norm(typ), default=default,
                           flags=flags, args=[norm(v) for v in a],
                           source=path, revision=rev, anchor=macro,
                           expected=raw[m.start():end], start=m.start(), end=end))
    return result


READERS = {'11':['app/config/gimprc.c#gimp_rc_new','app/config/gimprc-deserialize.c#gimp_rc_deserialize'],
           '12':['app/core/gimptooloptions.c#gimp_tool_options_deserialize'],
           '13':['app/core/gimpcontext.c#gimp_context_deserialize_property'],
           '14':['app/config/gimprc-deserialize.c#gimp_rc_deserialize'],
           '15':['app/tools/gimp-tools.c#gimp_tools_deserialize'],
           '16':['app/core/gimptoolpreset-load.c#gimp_tool_preset_load','app/core/gimpdynamics-load.c#gimp_dynamics_load']}
WRITERS = {'11':['app/config/gimprc.c#gimp_rc_save','app/config/gimprc-serialize.c#gimp_rc_serialize'],
           '12':['app/core/gimptooloptions.c#gimp_tool_options_serialize'],
           '13':['app/core/gimpcontext.c#gimp_context_serialize_property'],
           '14':['app/config/gimprc-serialize.c#gimp_rc_serialize'],
           '15':['app/tools/gimp-tools.c#gimp_tools_serialize'],
           '16':['app/core/gimptoolpreset-save.c#gimp_tool_preset_save','app/core/gimpdynamics-save.c#gimp_dynamics_save']}


def evidence(r):
    return {k:r[k] for k in ('source','revision','anchor','expected')} | {'path':r['source']}


def expected_registration_fields(src):
    revisions = {}
    for rev in ('native','legacy'):
        records = {}
        for path, route in source_routes(src,rev).items():
            for r in registrations(src,path,rev):
                r['route'] = '14' if r['name'].startswith('layer-presets-path') else route
                key = (r['route'],r['owner'],r['name'])
                assert key not in records, ('duplicate registration',rev,key)
                records[key] = r
        if rev == 'legacy':
            factory = src.read('app/presets/preset-factory.cpp', rev)
            layer = src.read('app/presets/layer-preset.cpp', rev)
            for suffix,variable,fn in [('','readonly','data'),('-writable','writable','writable')]:
                name='layer-presets-path'+suffix
                assert 'return "'+name+'";' in layer
                default='gimp_config_build_'+fn+'_path ("layer-presets")'
                assert default in layer
                candidates=[]
                for call in re.finditer(r'gimp_param_spec_config_path\s*\(',factory):
                    args,end=call_args(factory,call.start())
                    if args[0]==variable+'_prop': candidates.append((call,args,end))
                assert len(candidates)==1
                call,args,end=candidates[0]
                flags=norm(args[-1]);assert 'GIMP_CONFIG_PARAM_RESTART' in flags
                r=dict(name=name,owner='GimpCoreConfig',kind='config_path',type='gchar* (GIMP_CONFIG_PATH_DIR_LIST)',default=default,flags=flags,args=[name]+[norm(a) for a in args[1:]],source='app/presets/preset-factory.cpp',revision=rev,anchor='gimp_param_spec_config_path',expected=factory[call.start():end],start=call.start(),end=end,route='14')
                records[('14','GimpCoreConfig',name)]=r
        revisions[rev] = records
    fields = []
    for key in sorted(set(revisions['native']) | set(revisions['legacy'])):
        n,l = (revisions[v].get(key) for v in ('native','legacy'))
        r=n or l; route,owner,name = key
        fid='config.'+route+'.'+owner+'.'+name
        def_summary = '; '.join(v + ': ' + (x['default'] if x else 'no property registration in this revision') for v,x in [('native',n),('legacy',l)])
        flags = '; '.join(v + ': ' + (x['flags'] if x else 'not installed') for v,x in [('native',n),('legacy',l)])
        if route=='13': flags += '; context->serialize_props bit must include the property; writer emits changed properties'
        disposition = 'unchanged-standard' if n and l and n['type']==l['type'] and n['default']==l['default'] else 'saved-field'
        ignored = all(not x or 'GIMP_CONFIG_PARAM_IGNORE' in x['flags'] for x in (n,l))
        if ignored: disposition = 'unchanged-standard'; flags += '; read-compatible ignored property: never emitted'
        absence = 'Missing member leaves the existing object property unchanged. A newly constructed property starts at the exact registered default above; system/user gimprc overlay and existing context are not reset by absence.'
        readers=list(READERS[route])
        if owner=='GimpCoreConfig' and name in ('language','win32-pointer-input-api'):
            readers.append('app/config/gimpearlyrc.c#gimp_early_rc_constructed')
        fields.append(dict(id=fid,route=route,disposition=disposition,
          storage={'11':'gimprc GimpConfig S-expression','12':'tool-options/<tool> GimpConfig S-expression','13':'context resource-name GimpConfig S-expression','14':'gimprc preset folder preferences','15':'toolrc GimpToolItem/GimpToolGroup tree','16':'.gtp/.gdyn nested GimpConfig S-expression'}[route],
          version='native GIMP 3.0 source and legacy afa43fae GIMP 2.8; standard GimpConfig schema', saved_name=name,
          native_type=n['type'] if n else 'not registered; legacy schema member',legacy_type=l['type'] if l else 'not registered in legacy schema',
          owner=owner,default=def_summary,absence=absence,flags=flags,
          reader=readers,writer=WRITERS[route],legacy_reader=READERS[route],legacy_writer=WRITERS[route],
          evidence=[evidence(x) for x in (n,l) if x],assertion=fid+'.registration',test_file=TEST_FILE,
          registration={v:{k:x[k] for k in ('source','owner','name','kind','type','default','flags','args')} for v,x in [('native',n),('legacy',l)] if x}))
    return fields


def schema_domains(src,fields):
    """Polymorphic/inherited graphs have finite source-derived declared members.
    Instances are parameterized by actual persisted owner/type, not expanded
    into a misleading Cartesian product of every tool and every property.
    """
    domains=[]
    for rev in ('native','legacy'):
        parents={}
        for path in sorted(set(source_routes(src,rev))|{'app/config/gimprc.c'}):
            raw=src.read(path,rev)
            for match in re.finditer(r'G_DEFINE_(?:ABSTRACT_)?TYPE\w*\s*\(',raw):
                args,_=call_args(raw,match.start())
                if len(args)>=3:parents[norm(args[0])]=norm(args[2])
        domains.append(dict(revision=rev,inheritance=parents))
    tool_ids=sorted(f['id'] for f in fields if f['route'] in ('12','13') or f['owner']=='GimpViewable')
    dynamics_outputs=sorted(f['saved_name'] for f in fields if f['owner']=='GimpDynamics' and f['saved_name'].endswith('-output'))
    curve_slots=sorted(f['saved_name'] for f in fields if f['owner']=='GimpDynamicsOutput' and f['saved_name'].endswith('-curve'))
    domains += [
        dict(id='tool-preset-options',route='16',type_token='quoted registered GimpToolOptions subclass GType name',members=tool_ids,
             default='tool-options NULL at construction; omitted tool-options makes loader reject corrupt preset. On native typed-object load, brush/dynamics/mybrush/painter-mybrush/pattern/gradient/palette/font initialize from current user context before supplied properties overlay. Remaining properties start at each exact class registration/default. Reader repairs tool/tool-info and preserves declared serialize_props.',
             type_rule='GimpTransformOptions is remapped to GimpTransformGridOptions in native reader. GimpPainterDeviceOptions is explicitly admitted. Other names must resolve and derive from GimpToolOptions.',
             source='app/core/gimptoolpreset.c#gimp_tool_preset_deserialize_property'),
        dict(id='dynamics-output-curves',route='16',output_members=dynamics_outputs,curve_members=curve_slots,
             output_fields=sorted(f['id'] for f in fields if f['owner']=='GimpDynamicsOutput'),
             curve_fields=sorted(f['id'] for f in fields if f['owner']=='GimpCurve'),
             default='Every output slot constructs GimpDynamicsOutput with its own output-type enum; each input curve constructs GimpCurve. Scalar flags and exact construct arrays are the separately enumerated owner fields.',
             source='app/core/gimpdynamics.c#gimp_dynamics_create_output'),
        dict(id='gimprc-aggregates',route='11',members=sorted(f['id'] for f in fields if f['route']=='11'),
             nested_owners=['GimpTemplate','GimpGrid','GimpColorConfig','GimpDisplayOptions','GimpDisplayOptionsFullscreen','GimpDisplayOptionsNoImage','GimpFillOptions','GimpStrokeOptions'],
             default='System then user gimprc overlay existing defaults; object aggregate constructors and exact per-class registrations are separate records. Fill/stroke inherited foreground/background/pattern values are user-context-backed and serialize_props=0 in this aggregate.')]
    return domains


def expected_candidates(src,fields):
    manual={f['id']:f for f in fields}
    candidates=[]
    routes=source_routes(src,'legacy')
    reg_cache={}
    for row in csv.DictReader(open(src.root/'migration/inventory/persistence-sites.tsv'),delimiter='\t'):
     path,line=row['legacy_site'].split(':');line=int(line)
     if path.startswith('app/xcf/') or path in ('app/core/gimpimage.c','app/core/gimpimage.h','app/core/gimpitem.c','app/core/gimpitem.h','libgimpconfig/gimpconfig-params.h') or 'mypaint' in path:continue
     raw=src.read(path,'legacy');lines=raw.splitlines();assert row['source_line'].strip()==lines[line-1].strip(),row
     rec=dict(legacy_site=row['legacy_site'],route='none',disposition='non-persisted',fields=[],reason='',evidence=[dict(path=path,revision='legacy',anchor=row['operation'],expected=lines[line-1])])
     regs=[]
     if path in routes:
      if path not in reg_cache:reg_cache[path]=registrations(src,path,'legacy')
      regs=reg_cache[path];pos=sum(len(x)+1 for x in lines[:line-1])
      matches=[r for r in regs if r['start']<=pos+len(lines[line-1])-len(lines[line-1].lstrip())<r['end']]
      if matches:
       assert len(matches)==1,(row,matches); r=matches[0];route=routes[path];fid='config.'+route+'.'+r['owner']+'.'+r['name'];assert fid in manual,fid
       rec.update(route=route,disposition=manual[fid]['disposition'],fields=[fid],reason='Concrete installed GParamSpec declaration; source-parsed name/type/default/owner/flags reconciled in both revisions.')
     if not rec['reason']:
      if path=='app/base/json-cxx-utils.hpp':rec.update(route='10',disposition='dynamic-schema',fields=['config.10.preset.$.*'],reason='Generic JSON member-name builder; no independently fixed key. Full preset root dynamic schema preserves arbitrary members.')
      elif path=='app/core/gimp.c':
       if line==215:rec.update(route='06',disposition='dynamic-schema',fields=['config.06.parasite.records'],reason='Allocates empty global parasite list; not another wire member.')
       else:rec.update(route='05',disposition='saved-field',fields=['xcf.record.parasite._name','xcf.record.parasite._flags','xcf.record.parasite._size','xcf.record.parasite.data'],reason='Creates persistent gimp-comment image parasite from default-image comment; attach/free are lifetime calls for same route05 name/flags/size/data schema.')
      elif path=='app/core/gimpcontainer.c':rec.update(route='15',disposition='dynamic-schema',fields=['config.15.toolrc.item-type','config.15.toolrc.item-name','config.15.toolrc.item-order'],reason='Generic container decoder terminal framing check; concrete toolrc node type/name/order and registered properties cover this dispatch.')
      elif path=='app/core/gimptooloptions.c':rec.update(route='12',disposition='dynamic-schema',fields=[f['id'] for f in fields if f['route'] in ('12','13')],reason='Generic tool-options file reader/writer; concrete class properties plus inherited context are individually extracted.')
      elif path=='app/core/gimpcontext.c':rec.update(route='13',disposition='dynamic-schema',fields=[f['id'] for f in fields if f['route']=='13'],reason='Serialize-changed-properties dispatcher; exact installed context members and serialize_props mask define saved population.')
      elif path=='app/core/gimpcurve.c':rec.update(route='16',disposition='dynamic-schema',fields=[f['id'] for f in fields if f['owner']=='GimpCurve'],reason='Generic curve property serializer/decoder used by dynamics; concrete point/sample/type members and construct defaults are enumerated.')
      elif path=='app/dialogs/dialogs.c':rec.update(disposition='unchanged-standard',reason='Confirmed actual persistence of global_recent_docks to dockrc. This unchanged standard session/dock format is outside the fixed16-route field scope; not falsely classified as no disk IO.')
      elif path=='app/httpd/httpd.cpp':rec.update(reason='RESTResource::req_body_json parses transient HTTP request bytes; no saved resource/file schema or persistence writer at this use.')
      elif path=='app/file/file-open.c':rec.update(reason='Reads existing icc-profile image parasite to choose color-profile conversion; consumer-only, creates no new saved field. Persistent parasite framing is route05.')
      elif path=='app/widgets/gimpselectiondata.c':rec.update(reason='Transient GTK selection/clipboard curve serialization to/from strings. No durable route added; same GimpCurve values separately covered under dynamics route16.')
      elif path=='plug-ins/common/decompose.c':rec.update(reason='decompose-data parasite constructed with flags0, attached to image then freed; persistent bit absent, so not serialized to XCF.')
      elif path=='app/tools/gimp-tools.c':
       if line in (275,452):rec.update(reason='Disabled #if0 legacy flat toolrc compatibility code; active hierarchical toolrc dispatch separately covered.')
       else:rec.update(route='15',disposition='dynamic-schema',fields=[f['id'] for f in fields if f['route']=='15'],reason='Active hierarchical toolrc GimpContainer serializer/decoder; finite framing and GimpToolItem/GimpToolGroup registrations cover members.')
      else:raise AssertionError(('unclassified candidate',row))
     candidates.append(rec)
    return candidates


def validate_family(data, root):
    src=Source(root); expected=expected_registration_fields(src)
    # Explicit reader/writer schemas are separately asserted and independently
    # checked against all source-accessed keys below.
    expected += manual_fields(src,MANUAL_SPEC)
    actual={f['id']:f for f in data['fields']}; want={f['id']:f for f in expected}
    assert len(actual)==len(data['fields']), 'duplicate field IDs'
    assert actual.keys()==want.keys(), ('field population mismatch', sorted(want.keys()-actual.keys()),sorted(actual.keys()-want.keys()))
    for key in want:
        assert actual[key] == want[key], ('metadata mismatch', key, [k for k in set(actual[key])|set(want[key]) if actual[key].get(k)!=want[key].get(k)])
    validate_coverage(src, data, expected)
    candidate_expected={c['legacy_site']:c for c in expected_candidates(src,expected)}
    candidate_actual={c['legacy_site']:c for c in data['candidates']}
    assert len(candidate_actual)==len(data['candidates']) and candidate_actual==candidate_expected, 'candidate disposition or source population mismatch'
    assert data.get('schema_domains')==schema_domains(src,expected), 'inherited/parameterized schema domain mismatch'
    return [f['assertion'] for f in expected]


def validate_tool_group_retirement(text):
    """Static semantics for runtime membership retirement, not saved-field changes."""
    read = function_body(text, 'gimp_tools_deserialize')
    reset = function_body(text, 'gimp_tools_reset')
    clear = function_body(text, 'gimp_tools_clear_structure')
    copy = function_body(text, 'gimp_tools_copy_structure')
    retire = 'gimp_tools_clear_structure (gimp, container);'
    assert read.count(retire) == 1 and read.index(retire) < read.index('gimp_tools_copy_structure'), 'toolrc: retire before shared tool reuse'
    assert reset.count(retire) == 2, 'toolrc: reset and failed-read cleanup retire membership'
    assert 'gimp_tools_copy_structure (gimp, gimp->tool_info_list, container, NULL);' in reset, 'toolrc: default order fallback retained'
    assert 'file_version != TOOL_RC_FILE_VERSION && file_version != GIMP_PAINTER_MIGRATED_TOOLRC_VERSION' in read, 'toolrc: accepted versions unchanged'
    assert 'gimp_get_tool_info_iter (gimp)' in clear, 'toolrc: include orphan retained groups'
    lease = 'g_object_ref (parent);'
    remove = 'gimp_container_remove (gimp_viewable_get_children (parent), GIMP_OBJECT (tool_info));'
    release = 'g_object_unref (parent);'
    empty = 'gimp_container_clear (container);'
    assert all(clear.count(x) == 1 for x in (lease, remove, release, empty)), 'toolrc: exact owner lease and notified removal'
    assert clear.index(lease) < clear.index(remove) < clear.index(release) < clear.index(empty), 'toolrc: detach while leased before root clear'
    assert 'gimp_tool_group_set_active_tool (GIMP_TOOL_GROUP (dest_tool_item), active);' in copy, 'toolrc: active group selection preserved'
    assert 'gimp_viewable_set_expanded (GIMP_VIEWABLE (dest_tool_item), gimp_viewable_get_expanded (GIMP_VIEWABLE (src_tool_item)));' in copy, 'toolrc: expansion preserved'
    assert 'gimp_tool_item_set_visible ( dest_tool_item, gimp_tool_item_get_visible (src_tool_item));' in copy, 'toolrc: visibility preserved'


def tool_group_negative_controls(root):
    """Read-only source overlays, checked semantically before program hashes."""
    text = (Path(root) / 'app/tools/gimp-tools.c').read_text()
    validate_tool_group_retirement(text)
    controls = []
    mutations = [
      ('reader-retirement', 'gimp_tools_clear_structure (gimp, container);', 'gimp_container_clear (container);'),
      ('parent-lease', 'g_object_ref (parent);', '(void) parent;'),
      ('notified-detach', 'gimp_container_remove (gimp_viewable_get_children (parent),', 'gimp_container_have (gimp_viewable_get_children (parent),'),
      ('lease-release-order', 'g_object_ref (parent);', 'g_object_unref (parent);'),
      ('default-order', 'gimp_tools_copy_structure (gimp, gimp->tool_info_list, container, NULL);', '(void) container;'),
      ('accepted-version', 'file_version != GIMP_PAINTER_MIGRATED_TOOLRC_VERSION', 'file_version == GIMP_PAINTER_MIGRATED_TOOLRC_VERSION'),
      ('group-selection', 'gimp_tool_group_set_active_tool (GIMP_TOOL_GROUP (dest_tool_item), active);', '(void) active;'),
      ('group-expansion', 'gimp_viewable_get_expanded (GIMP_VIEWABLE (src_tool_item))', 'FALSE'),
    ]
    for name, old, new in mutations:
        assert old in text, ('missing mutation anchor', name)
        changed = text.replace(old, new, 1)
        try: validate_tool_group_retirement(changed)
        except AssertionError as error:
            controls.append(dict(id='config:tool-group:'+name, rejected=True, reason=str(error)))
        else: raise AssertionError('negative control accepted: '+name)
    return controls


def validate_json_resource_ownership(resource,preset,header):
    setter=function_body(resource,'gimp_json_resource_set_json')
    assert setter.index('g_object_ref (resource)') < setter.index('klass->prepare_root'), 'normalization owner lease'
    assert '|| private->disposed' in setter, 'normalization terminal check'
    assert 'private->root = owned;' in setter, 'single root publication'
    assert 'if (owned) json_node_ref (owned);' in setter, 'metadata root lease'
    assert setter.index('klass->root_changed') < setter.index('g_object_notify'), 'metadata before public root notification'
    assert 'if (!private->disposed) g_object_notify' in setter, 'no notification after disposal'
    prepare=function_body(preset,'prepare_root')
    assert '!root || !JSON_NODE_HOLDS_OBJECT (root)' in prepare, 'specialized object validation'
    assert '*owned = deep_copy (root);' in prepare, 'specialized complete deep copy'
    assert 'JsonNode *root;' not in header and 'GimpJsonResource parent_instance;' in header, 'one inherited state'
    klass=function_body(preset,'gimp_layer_preset_class_init')
    assert '->prepare_root = prepare_root' in klass and '->root_changed = root_changed' in klass, 'specialized policy installed'
    assert 'gimp_layer_preset_dup_json' in function_body(preset,'save'), 'save detached snapshot'
    changed=function_body(preset,'root_changed')
    assert 'gimp_object_set_name' in changed and 'gimp_json_resource_is_live' in changed and 'gimp_data_dirty' in changed, 'name and dirty policy'


def json_resource_negative_controls(root):
    root=Path(root)
    files=[(root/path).read_text() for path in ['app/core/gimpjsonresource.c','app/core/gimplayerpreset.c','app/core/gimplayerpreset.h']]
    changes=[('owner-lease',0,'g_object_ref (resource);','/* no lease */'),
             ('terminal-check',0,'|| private->disposed','|| FALSE'),
             ('root-publication',0,'private->root = owned;','private->root = NULL;'),
             ('metadata-root-lease',0,'if (owned) json_node_ref (owned);','/* no root lease */'),
             ('terminal-notify',0,'if (!private->disposed) g_object_notify','if (TRUE) g_object_notify'),
             ('object-validation',1,'!JSON_NODE_HOLDS_OBJECT (root)','FALSE'),
             ('null-root-validation',1,'!root || !JSON_NODE_HOLDS_OBJECT (root)','root && !JSON_NODE_HOLDS_OBJECT (root)'),
             ('deep-copy',1,'*owned = deep_copy (root);','*owned = root ? json_node_ref (root) : NULL;'),
             ('duplicate-state',2,'GimpJsonResource parent_instance;','GimpJsonResource parent_instance; JsonNode *root;')]
    result=[]
    for name,index,old,new in changes:
        assert old in files[index], name
        candidate=files.copy();candidate[index]=candidate[index].replace(old,new,1)
        try:validate_json_resource_ownership(*candidate)
        except (AssertionError,ValueError) as error:result.append(dict(id='config:json-resource:'+name,rejected=True,reason=str(error)))
        else:raise AssertionError('JSON ownership mutation accepted: '+name)
    return result


def manual_fields(src, schema):
    validate_tool_group_retirement(src.read('app/tools/gimp-tools.c'))
    validate_json_resource_ownership(src.read('app/core/gimpjsonresource.c'),src.read('app/core/gimplayerpreset.c'),src.read('app/core/gimplayerpreset.h'))
    # Finite JSON key population is independently read from both source
    # consumers; unknown root members are the separately explicit open schema.
    native=src.read('app/core/gimplayerpreset-apply.cpp')
    legacy=src.read('app/presets/layer-preset.cpp','legacy')
    expected={('native',norm(v),k) for v,k in re.findall(r'member\s*\(\s*(node|arg|filter|object|root\.get\s*\(\s*\)|preset->root)\s*,\s*"([^"]+)"',native)}
    expected|={('legacy',v,k) for v,k in re.findall(r'\b(\w+)\s*\[\s*"([^"]+)"\s*\]',legacy)}
    expected|={('legacy','key',k) for k in re.findall(r'strcmp\(key, "([^"]+)"\)',legacy)}
    actual={tuple(k) for f in schema['fields'] for k in f.get('source_keys',[])}
    assert actual==expected, ('finite preset source-key coverage',sorted(expected-actual),sorted(actual-expected))
    # Fixed parasite decoder arguments are checked independently of field rows.
    for rev in ('native','legacy'):
        parasite=src.read('app/core/gimpparasitelist.c',rev)
        parsed=set(re.findall(r'gimp_scanner_parse_(?:string|int|data)\s*\(\s*scanner,\s*(?:parasite_data_size,\s*)?&(parasite_\w+)',parasite))
        assert parsed=={'parasite_name','parasite_flags','parasite_data_size','parasite_data'}, ('parasite parser members',rev,parsed)
        assert 'gimp_parasite_is_persistent (parasite)' in parasite
    assert {f['saved_name'] for f in schema['fields'] if f['route']=='06'}=={'parasite[]','parasite[].name','parasite[].flags','parasite[].size','parasite[].data'}
    for program in schema.get('programs',[]):
        actual_program=function_body(src.read(program['path'],program['revision']),program['function'])
        assert hashlib.sha256(actual_program.encode()).hexdigest()==program['sha256'], ('scoped reader/writer rule changed',program['path'],program['function'])
    fields=[]
    for f in schema['fields']:
        for ev in f['evidence']:
            text=src.read(ev['path'],ev['revision'])
            assert ev['expected'] in text, ('manual source mapping changed',f['id'],ev['path'],ev['anchor'])
        fields.append(f)
    return fields


def validate_coverage(src,data,fields):
    early=registrations(src,'app/config/gimpearlyrc.c','native')
    core=registrations(src,'app/config/gimpcoreconfig.c','native')
    for e in early:
        c=[c for c in core if c['name']==e['name']]
        assert len(c)==1 and e['type']==c[0]['type'] and e['default']==c[0]['default'], ('early gimprc reader differs',e['name'])
    for f in fields:
        for side in ('reader','writer','legacy_reader','legacy_writer'):
            rev='legacy' if side.startswith('legacy_') else 'native'
            for anchor in f[side]:
                path,token=anchor.split('#',1)
                assert token in src.read(path,rev), ('unresolved reader/writer',f['id'],side,anchor)
    # Candidate completeness and exact classifications are checked in parent
    # across all route workers; each registration has its original source site.


def main():
    p=argparse.ArgumentParser();p.add_argument('--root',default=str(Path(__file__).resolve().parents[2]));p.add_argument('--family');p.add_argument('--emit');args=p.parse_args()
    src=Source(args.root)
    if args.emit:
        data={'fields':expected_registration_fields(src)}
        Path(args.emit).write_text(json.dumps(data,indent=2)+'\n')
        print('extracted',len(data['fields']),'independently parsed registered members')
    else:
        path=Path(args.family) if args.family else Path(args.root)/'migration/inventory/persistence-config-fields.json'
        ids=validate_family(json.loads(path.read_text()),args.root)
        print(json.dumps({'result':'PASS','assertions':len(ids),'scope':'static source metadata; no runtime roundtrip claim'}))
MANUAL_SPEC = json.loads(r'''{
  "fields": [
    {
      "id": "config.10.preset.$.*",
      "route": "10",
      "disposition": "dynamic-schema",
      "storage": "layer-preset JSON",
      "version": "native GIMP3 source; legacy afa43fae source",
      "saved_name": "$.*",
      "native_type": "arbitrary recursive JsonNode member/value",
      "legacy_type": "arbitrary recursive JsonNode member/value",
      "owner": "GimpJsonResource private root, inherited by GimpLayerPreset (native); GimpJsonResource.root (legacy)",
      "default": "New native object starts with version1, source-layer.type any, replacement-layer empty array; legacy JSON object retains the loaded root without rebuilding.",
      "absence": "Unknown or absent members are not synthesized except native root.name during save.",
      "flags": "Complete JSON tree saved; only root.name is synchronized from resource name on native save. Apply interprets the paths below; retention does not imply successful application.",
      "reader": [
        "app/core/gimplayerpreset-apply.cpp#gimp_layer_preset_apply_full",
        "app/core/gimplayerpreset.c#gimp_layer_preset_load"
      ],
      "writer": [
        "app/core/gimplayerpreset.c#save"
      ],
      "legacy_reader": [
        "app/presets/layer-preset.cpp#LayerPresetApplier",
        "app/presets/gimpjsonresource.cpp#JsonResource::load"
      ],
      "legacy_writer": [
        "app/presets/gimpjsonresource.cpp#JsonResource::save"
      ],
      "evidence": [
        {
          "path": "app/core/gimpjsonresource.c",
          "revision": "native",
          "anchor": "private->root = owned;",
          "expected": "  private->root = owned;"
        },
        {
          "path": "app/core/gimplayerpreset.c",
          "revision": "native",
          "anchor": "json_generator_set_root (generator, snapshot);",
          "expected": "  json_generator_set_root (generator, snapshot);"
        },
        {
          "path": "app/presets/gimpjsonresource.cpp",
          "revision": "legacy",
          "anchor": "generator [json_generator_set_root] (root);",
          "expected": "  generator [json_generator_set_root] (root);"
        },
        {
          "path": "app/core/gimplayerpreset.c",
          "revision": "native",
          "anchor": "json_parser_load_from_data (parser, \"{\\\"version\\\":1",
          "expected": "  json_parser_load_from_data (parser, \"{\\\"version\\\":1,\\\"source-layer\\\":{\\\"type\\\":\\\"any\\\"},\\\"replacement-layer\\\":[]}\", -1, NULL);"
        },
        {
          "path": "app/presets/gimpjsonresource.cpp",
          "revision": "legacy",
          "anchor": "root = node;",
          "expected": "    root = node;"
        }
      ],
      "assertion": "config.10.preset.$.*.schema",
      "test_file": "migration/tests/test_persistence_config_contract.py"
    },
    {
      "id": "config.10.preset.name",
      "route": "10",
      "disposition": "saved-field",
      "storage": "layer-preset JSON",
      "version": "native GIMP3 source; legacy afa43fae source",
      "saved_name": "name",
      "native_type": "JSON string synchronized from GimpObject name",
      "legacy_type": "JSON string, name read from root but writer emits original root",
      "owner": "GimpJsonResource private root, inherited by GimpLayerPreset (native); GimpJsonResource.root (legacy)",
      "default": "Native load begins with file basename; string name overrides. Native save sets root.name to current non-NULL resource name. Legacy reads string name if present.",
      "absence": "Native load keeps basename for missing/non-string name; legacy does not override initial resource name.",
      "flags": "Complete JSON tree saved; only root.name is synchronized from resource name on native save. Apply interprets the paths below; retention does not imply successful application.",
      "reader": [
        "app/core/gimplayerpreset-apply.cpp#gimp_layer_preset_apply_full",
        "app/core/gimplayerpreset.c#gimp_layer_preset_load"
      ],
      "writer": [
        "app/core/gimplayerpreset.c#save"
      ],
      "legacy_reader": [
        "app/presets/layer-preset.cpp#LayerPresetApplier",
        "app/presets/gimpjsonresource.cpp#JsonResource::load"
      ],
      "legacy_writer": [
        "app/presets/gimpjsonresource.cpp#JsonResource::save"
      ],
      "evidence": [
        {
          "path": "app/core/gimpjsonresource.c",
          "revision": "native",
          "anchor": "private->root = owned;",
          "expected": "  private->root = owned;"
        },
        {
          "path": "app/core/gimplayerpreset.c",
          "revision": "native",
          "anchor": "json_generator_set_root (generator, snapshot);",
          "expected": "  json_generator_set_root (generator, snapshot);"
        },
        {
          "path": "app/presets/gimpjsonresource.cpp",
          "revision": "legacy",
          "anchor": "generator [json_generator_set_root] (root);",
          "expected": "  generator [json_generator_set_root] (root);"
        },
        {
          "path": "app/core/gimplayerpreset.c",
          "revision": "native",
          "anchor": "json_object_set_string_member",
          "expected": "    json_object_set_string_member (json_node_get_object (snapshot), \"name\", gimp_object_get_name (data));"
        },
        {
          "path": "app/core/gimplayerpreset.c",
          "revision": "native",
          "anchor": "gchar *basename",
          "expected": "      gchar *basename = g_file_get_basename (file);"
        },
        {
          "path": "app/presets/gimpjsonresource.cpp",
          "revision": "legacy",
          "anchor": "if (json.is_object() && json.has(\"name\")",
          "expected": "  if (json.is_object() && json.has(\"name\") && (const gchar*)json[\"name\"])"
        }
      ],
      "assertion": "config.10.preset.name.schema",
      "test_file": "migration/tests/test_persistence_config_contract.py"
    },
    {
      "id": "config.10.preset.version",
      "route": "10",
      "disposition": "saved-field",
      "storage": "layer-preset JSON",
      "version": "native GIMP3 source; legacy afa43fae source",
      "saved_name": "version",
      "native_type": "JSON finite integral gint exactly1 for apply",
      "legacy_type": "Opaque JSON scalar retained; legacy applier does not inspect version",
      "owner": "GimpJsonResource private root, inherited by GimpLayerPreset (native); GimpJsonResource.root (legacy)",
      "default": "Native new root1; loaded value is never replaced.",
      "absence": "Native apply fails missing/NULL/nonintegral/other version; legacy applies without a version gate.",
      "flags": "Complete JSON tree saved; only root.name is synchronized from resource name on native save. Apply interprets the paths below; retention does not imply successful application.",
      "reader": [
        "app/core/gimplayerpreset-apply.cpp#gimp_layer_preset_apply_full",
        "app/core/gimplayerpreset.c#gimp_layer_preset_load"
      ],
      "writer": [
        "app/core/gimplayerpreset.c#save"
      ],
      "legacy_reader": [
        "app/presets/layer-preset.cpp#LayerPresetApplier",
        "app/presets/gimpjsonresource.cpp#JsonResource::load"
      ],
      "legacy_writer": [
        "app/presets/gimpjsonresource.cpp#JsonResource::save"
      ],
      "evidence": [
        {
          "path": "app/core/gimpjsonresource.c",
          "revision": "native",
          "anchor": "private->root = owned;",
          "expected": "  private->root = owned;"
        },
        {
          "path": "app/core/gimplayerpreset.c",
          "revision": "native",
          "anchor": "json_generator_set_root (generator, snapshot);",
          "expected": "  json_generator_set_root (generator, snapshot);"
        },
        {
          "path": "app/presets/gimpjsonresource.cpp",
          "revision": "legacy",
          "anchor": "generator [json_generator_set_root] (root);",
          "expected": "  generator [json_generator_set_root] (root);"
        },
        {
          "path": "app/core/gimplayerpreset-apply.cpp",
          "revision": "native",
          "anchor": "if (integer (member (root.get (), \"version\")) != 1)",
          "expected": "    if (integer (member (root.get (), \"version\")) != 1) fail (\"Unsupported layer preset version\");"
        },
        {
          "path": "app/presets/layer-preset.cpp",
          "revision": "legacy",
          "anchor": "auto json     = JSON::ref(ipreset->get_json());",
          "expected": "    auto json     = JSON::ref(ipreset->get_json());"
        }
      ],
      "assertion": "config.10.preset.version.schema",
      "test_file": "migration/tests/test_persistence_config_contract.py",
      "source_keys": [
        [
          "native",
          "root.get ()",
          "version"
        ]
      ]
    },
    {
      "id": "config.10.preset.source-layer",
      "route": "10",
      "disposition": "saved-field",
      "storage": "layer-preset JSON",
      "version": "native GIMP3 source; legacy afa43fae source",
      "saved_name": "source-layer",
      "native_type": "JSON object",
      "legacy_type": "JSON object",
      "owner": "GimpJsonResource private root, inherited by GimpLayerPreset (native); GimpJsonResource.root (legacy)",
      "default": "Native new root {type:any}.",
      "absence": "Missing/non-object rejects source match in both revisions.",
      "flags": "Complete JSON tree saved; only root.name is synchronized from resource name on native save. Apply interprets the paths below; retention does not imply successful application.",
      "reader": [
        "app/core/gimplayerpreset-apply.cpp#gimp_layer_preset_apply_full",
        "app/core/gimplayerpreset.c#gimp_layer_preset_load"
      ],
      "writer": [
        "app/core/gimplayerpreset.c#save"
      ],
      "legacy_reader": [
        "app/presets/layer-preset.cpp#LayerPresetApplier",
        "app/presets/gimpjsonresource.cpp#JsonResource::load"
      ],
      "legacy_writer": [
        "app/presets/gimpjsonresource.cpp#JsonResource::save"
      ],
      "evidence": [
        {
          "path": "app/core/gimpjsonresource.c",
          "revision": "native",
          "anchor": "private->root = owned;",
          "expected": "  private->root = owned;"
        },
        {
          "path": "app/core/gimplayerpreset.c",
          "revision": "native",
          "anchor": "json_generator_set_root (generator, snapshot);",
          "expected": "  json_generator_set_root (generator, snapshot);"
        },
        {
          "path": "app/presets/gimpjsonresource.cpp",
          "revision": "legacy",
          "anchor": "generator [json_generator_set_root] (root);",
          "expected": "  generator [json_generator_set_root] (root);"
        },
        {
          "path": "app/core/gimplayerpreset-apply.cpp",
          "revision": "native",
          "anchor": "matches (member (root.get (), \"source-layer\"), source)",
          "expected": "    if (!matches (member (root.get (), \"source-layer\"), source)) fail (\"Selected layer does not match this layer preset\");"
        },
        {
          "path": "app/presets/layer-preset.cpp",
          "revision": "legacy",
          "anchor": "auto source_json = json[\"source-layer\"];",
          "expected": "    auto source_json = json[\"source-layer\"];"
        }
      ],
      "assertion": "config.10.preset.source-layer.schema",
      "test_file": "migration/tests/test_persistence_config_contract.py",
      "source_keys": [
        [
          "native",
          "root.get ()",
          "source-layer"
        ],
        [
          "native",
          "root.get ()",
          "source-layer"
        ],
        [
          "legacy",
          "json",
          "source-layer"
        ]
      ]
    },
    {
      "id": "config.10.preset.source-layer.type",
      "route": "10",
      "disposition": "saved-field",
      "storage": "layer-preset JSON",
      "version": "native GIMP3 source; legacy afa43fae source",
      "saved_name": "source-layer.type",
      "native_type": "JSON string: any/normal/group/clone/filter",
      "legacy_type": "JSON string: any/normal/group/clone/filter",
      "owner": "GimpJsonResource private root, inherited by GimpLayerPreset (native); GimpJsonResource.root (legacy)",
      "default": "Native missing or null string fallback any; legacy absence imposes no type restriction.",
      "absence": "Native any fallback; legacy loops only present keys.",
      "flags": "Complete JSON tree saved; only root.name is synchronized from resource name on native save. Apply interprets the paths below; retention does not imply successful application.",
      "reader": [
        "app/core/gimplayerpreset-apply.cpp#gimp_layer_preset_apply_full",
        "app/core/gimplayerpreset.c#gimp_layer_preset_load"
      ],
      "writer": [
        "app/core/gimplayerpreset.c#save"
      ],
      "legacy_reader": [
        "app/presets/layer-preset.cpp#LayerPresetApplier",
        "app/presets/gimpjsonresource.cpp#JsonResource::load"
      ],
      "legacy_writer": [
        "app/presets/gimpjsonresource.cpp#JsonResource::save"
      ],
      "evidence": [
        {
          "path": "app/core/gimpjsonresource.c",
          "revision": "native",
          "anchor": "private->root = owned;",
          "expected": "  private->root = owned;"
        },
        {
          "path": "app/core/gimplayerpreset.c",
          "revision": "native",
          "anchor": "json_generator_set_root (generator, snapshot);",
          "expected": "  json_generator_set_root (generator, snapshot);"
        },
        {
          "path": "app/presets/gimpjsonresource.cpp",
          "revision": "legacy",
          "anchor": "generator [json_generator_set_root] (root);",
          "expected": "  generator [json_generator_set_root] (root);"
        },
        {
          "path": "app/core/gimplayerpreset-apply.cpp",
          "revision": "native",
          "anchor": "const char *type = string (member (node, \"type\"), \"any\");",
          "expected": "  const char *type = string (member (node, \"type\"), \"any\");"
        },
        {
          "path": "app/presets/layer-preset.cpp",
          "revision": "legacy",
          "anchor": "if (strcmp(key, \"type\") == 0)",
          "expected": "      if (strcmp(key, \"type\") == 0) {"
        }
      ],
      "assertion": "config.10.preset.source-layer.type.schema",
      "test_file": "migration/tests/test_persistence_config_contract.py",
      "source_keys": [
        [
          "native",
          "node",
          "type"
        ],
        [
          "legacy",
          "key",
          "type"
        ]
      ]
    },
    {
      "id": "config.10.preset.source-layer.alpha",
      "route": "10",
      "disposition": "saved-field",
      "storage": "layer-preset JSON",
      "version": "native GIMP3 source; legacy afa43fae source",
      "saved_name": "source-layer.alpha",
      "native_type": "JSON boolean",
      "legacy_type": "JSON boolean coercion by legacy JSON::INode",
      "owner": "GimpJsonResource private root, inherited by GimpLayerPreset (native); GimpJsonResource.root (legacy)",
      "default": "No restriction when absent.",
      "absence": "Native requires boolean and compares source alpha; legacy only checks when key exists.",
      "flags": "Complete JSON tree saved; only root.name is synchronized from resource name on native save. Apply interprets the paths below; retention does not imply successful application.",
      "reader": [
        "app/core/gimplayerpreset-apply.cpp#gimp_layer_preset_apply_full",
        "app/core/gimplayerpreset.c#gimp_layer_preset_load"
      ],
      "writer": [
        "app/core/gimplayerpreset.c#save"
      ],
      "legacy_reader": [
        "app/presets/layer-preset.cpp#LayerPresetApplier",
        "app/presets/gimpjsonresource.cpp#JsonResource::load"
      ],
      "legacy_writer": [
        "app/presets/gimpjsonresource.cpp#JsonResource::save"
      ],
      "evidence": [
        {
          "path": "app/core/gimpjsonresource.c",
          "revision": "native",
          "anchor": "private->root = owned;",
          "expected": "  private->root = owned;"
        },
        {
          "path": "app/core/gimplayerpreset.c",
          "revision": "native",
          "anchor": "json_generator_set_root (generator, snapshot);",
          "expected": "  json_generator_set_root (generator, snapshot);"
        },
        {
          "path": "app/presets/gimpjsonresource.cpp",
          "revision": "legacy",
          "anchor": "generator [json_generator_set_root] (root);",
          "expected": "  generator [json_generator_set_root] (root);"
        },
        {
          "path": "app/core/gimplayerpreset-apply.cpp",
          "revision": "native",
          "anchor": "JsonNode *alpha = member (node, \"alpha\");",
          "expected": "  JsonNode *alpha = member (node, \"alpha\");"
        },
        {
          "path": "app/presets/layer-preset.cpp",
          "revision": "legacy",
          "anchor": "} else if (strcmp(key, \"alpha\") == 0)",
          "expected": "      } else if (strcmp(key, \"alpha\") == 0) {"
        }
      ],
      "assertion": "config.10.preset.source-layer.alpha.schema",
      "test_file": "migration/tests/test_persistence_config_contract.py",
      "source_keys": [
        [
          "native",
          "node",
          "alpha"
        ],
        [
          "legacy",
          "key",
          "alpha"
        ]
      ]
    },
    {
      "id": "config.10.preset.replacement-layer",
      "route": "10",
      "disposition": "saved-field",
      "storage": "layer-preset JSON",
      "version": "native GIMP3 source; legacy afa43fae source",
      "saved_name": "replacement-layer",
      "native_type": "JSON array of layer objects",
      "legacy_type": "JSON array of layer objects",
      "owner": "GimpJsonResource private root, inherited by GimpLayerPreset (native); GimpJsonResource.root (legacy)",
      "default": "Native new root empty array.",
      "absence": "Native missing/non-array fails; legacy uses traverse on supplied array.",
      "flags": "Complete JSON tree saved; only root.name is synchronized from resource name on native save. Apply interprets the paths below; retention does not imply successful application.",
      "reader": [
        "app/core/gimplayerpreset-apply.cpp#gimp_layer_preset_apply_full",
        "app/core/gimplayerpreset.c#gimp_layer_preset_load"
      ],
      "writer": [
        "app/core/gimplayerpreset.c#save"
      ],
      "legacy_reader": [
        "app/presets/layer-preset.cpp#LayerPresetApplier",
        "app/presets/gimpjsonresource.cpp#JsonResource::load"
      ],
      "legacy_writer": [
        "app/presets/gimpjsonresource.cpp#JsonResource::save"
      ],
      "evidence": [
        {
          "path": "app/core/gimpjsonresource.c",
          "revision": "native",
          "anchor": "private->root = owned;",
          "expected": "  private->root = owned;"
        },
        {
          "path": "app/core/gimplayerpreset.c",
          "revision": "native",
          "anchor": "json_generator_set_root (generator, snapshot);",
          "expected": "  json_generator_set_root (generator, snapshot);"
        },
        {
          "path": "app/presets/gimpjsonresource.cpp",
          "revision": "legacy",
          "anchor": "generator [json_generator_set_root] (root);",
          "expected": "  generator [json_generator_set_root] (root);"
        },
        {
          "path": "app/core/gimplayerpreset-apply.cpp",
          "revision": "native",
          "anchor": "Replacement layer must be an array",
          "expected": "    if (!node || !JSON_NODE_HOLDS_ARRAY (node)) fail (\"Replacement layer must be an array\");"
        },
        {
          "path": "app/core/gimplayerpreset-apply.cpp",
          "revision": "native",
          "anchor": "member (root.get (), \"replacement-layer\")",
          "expected": "    application.layers (member (root.get (), \"replacement-layer\"), parent, index);"
        },
        {
          "path": "app/presets/layer-preset.cpp",
          "revision": "legacy",
          "anchor": "auto replace_json = json[\"replacement-layer\"];",
          "expected": "    auto replace_json = json[\"replacement-layer\"];"
        }
      ],
      "assertion": "config.10.preset.replacement-layer.schema",
      "test_file": "migration/tests/test_persistence_config_contract.py",
      "source_keys": [
        [
          "native",
          "root.get ()",
          "replacement-layer"
        ],
        [
          "legacy",
          "json",
          "replacement-layer"
        ]
      ]
    },
    {
      "id": "config.10.preset.replacement-layer[].target",
      "route": "10",
      "disposition": "saved-field",
      "storage": "layer-preset JSON",
      "version": "native GIMP3 source; legacy afa43fae source",
      "saved_name": "replacement-layer[].target",
      "native_type": "JSON string new/source",
      "legacy_type": "JSON string new/source",
      "owner": "GimpJsonResource private root, inherited by GimpLayerPreset (native); GimpJsonResource.root (legacy)",
      "default": "Native missing/null new; legacy null defaults new via target branch.",
      "absence": "Unknown non-null target fails native; legacy creates no target.",
      "flags": "Complete JSON tree saved; only root.name is synchronized from resource name on native save. Apply interprets the paths below; retention does not imply successful application.",
      "reader": [
        "app/core/gimplayerpreset-apply.cpp#gimp_layer_preset_apply_full",
        "app/core/gimplayerpreset.c#gimp_layer_preset_load"
      ],
      "writer": [
        "app/core/gimplayerpreset.c#save"
      ],
      "legacy_reader": [
        "app/presets/layer-preset.cpp#LayerPresetApplier",
        "app/presets/gimpjsonresource.cpp#JsonResource::load"
      ],
      "legacy_writer": [
        "app/presets/gimpjsonresource.cpp#JsonResource::save"
      ],
      "evidence": [
        {
          "path": "app/core/gimpjsonresource.c",
          "revision": "native",
          "anchor": "private->root = owned;",
          "expected": "  private->root = owned;"
        },
        {
          "path": "app/core/gimplayerpreset.c",
          "revision": "native",
          "anchor": "json_generator_set_root (generator, snapshot);",
          "expected": "  json_generator_set_root (generator, snapshot);"
        },
        {
          "path": "app/presets/gimpjsonresource.cpp",
          "revision": "legacy",
          "anchor": "generator [json_generator_set_root] (root);",
          "expected": "  generator [json_generator_set_root] (root);"
        },
        {
          "path": "app/core/gimplayerpreset-apply.cpp",
          "revision": "native",
          "anchor": "const char *target = string (member (object, \"target\"), \"new\");",
          "expected": "        const char *target = string (member (object, \"target\"), \"new\");"
        },
        {
          "path": "app/presets/layer-preset.cpp",
          "revision": "legacy",
          "anchor": "const gchar* target = node_json[\"target\"];",
          "expected": "    const gchar* target = node_json[\"target\"]; // *new, source"
        }
      ],
      "assertion": "config.10.preset.replacement-layer[].target.schema",
      "test_file": "migration/tests/test_persistence_config_contract.py",
      "source_keys": [
        [
          "native",
          "object",
          "target"
        ],
        [
          "legacy",
          "node_json",
          "target"
        ],
        [
          "legacy",
          "j",
          "target"
        ]
      ]
    },
    {
      "id": "config.10.preset.replacement-layer[].type",
      "route": "10",
      "disposition": "saved-field",
      "storage": "layer-preset JSON",
      "version": "native GIMP3 source; legacy afa43fae source",
      "saved_name": "replacement-layer[].type",
      "native_type": "JSON string normal/group/clone/filter",
      "legacy_type": "JSON string normal/group/clone/filter",
      "owner": "GimpJsonResource private root, inherited by GimpLayerPreset (native); GimpJsonResource.root (legacy)",
      "default": "Native missing/null normal; legacy null defaults normal.",
      "absence": "Only new target uses type for creation.",
      "flags": "Complete JSON tree saved; only root.name is synchronized from resource name on native save. Apply interprets the paths below; retention does not imply successful application.",
      "reader": [
        "app/core/gimplayerpreset-apply.cpp#gimp_layer_preset_apply_full",
        "app/core/gimplayerpreset.c#gimp_layer_preset_load"
      ],
      "writer": [
        "app/core/gimplayerpreset.c#save"
      ],
      "legacy_reader": [
        "app/presets/layer-preset.cpp#LayerPresetApplier",
        "app/presets/gimpjsonresource.cpp#JsonResource::load"
      ],
      "legacy_writer": [
        "app/presets/gimpjsonresource.cpp#JsonResource::save"
      ],
      "evidence": [
        {
          "path": "app/core/gimpjsonresource.c",
          "revision": "native",
          "anchor": "private->root = owned;",
          "expected": "  private->root = owned;"
        },
        {
          "path": "app/core/gimplayerpreset.c",
          "revision": "native",
          "anchor": "json_generator_set_root (generator, snapshot);",
          "expected": "  json_generator_set_root (generator, snapshot);"
        },
        {
          "path": "app/presets/gimpjsonresource.cpp",
          "revision": "legacy",
          "anchor": "generator [json_generator_set_root] (root);",
          "expected": "  generator [json_generator_set_root] (root);"
        },
        {
          "path": "app/core/gimplayerpreset-apply.cpp",
          "revision": "native",
          "anchor": "const char *type = string (member (object, \"type\"), \"normal\");",
          "expected": "        const char *type = string (member (object, \"type\"), \"normal\");"
        },
        {
          "path": "app/presets/layer-preset.cpp",
          "revision": "legacy",
          "anchor": "const gchar* type = node_json[\"type\"];",
          "expected": "        const gchar* type = node_json[\"type\"]; // *normal, group, filter, clone"
        }
      ],
      "assertion": "config.10.preset.replacement-layer[].type.schema",
      "test_file": "migration/tests/test_persistence_config_contract.py",
      "source_keys": [
        [
          "native",
          "object",
          "type"
        ],
        [
          "legacy",
          "node_json",
          "type"
        ]
      ]
    },
    {
      "id": "config.10.preset.replacement-layer[].name",
      "route": "10",
      "disposition": "saved-field",
      "storage": "layer-preset JSON",
      "version": "native GIMP3 source; legacy afa43fae source",
      "saved_name": "replacement-layer[].name",
      "native_type": "JSON string or null",
      "legacy_type": "JSON string or null",
      "owner": "GimpJsonResource private root, inherited by GimpLayerPreset (native); GimpJsonResource.root (legacy)",
      "default": "Native new normal/group/clone/filter names New Layer/New Group Layer/New Clone Layer/New Filter Layer when absent. Legacy absent NULL passed to corresponding constructors.",
      "absence": "Source target keeps its existing name; neither applier renames source from this field.",
      "flags": "Complete JSON tree saved; only root.name is synchronized from resource name on native save. Apply interprets the paths below; retention does not imply successful application.",
      "reader": [
        "app/core/gimplayerpreset-apply.cpp#gimp_layer_preset_apply_full",
        "app/core/gimplayerpreset.c#gimp_layer_preset_load"
      ],
      "writer": [
        "app/core/gimplayerpreset.c#save"
      ],
      "legacy_reader": [
        "app/presets/layer-preset.cpp#LayerPresetApplier",
        "app/presets/gimpjsonresource.cpp#JsonResource::load"
      ],
      "legacy_writer": [
        "app/presets/gimpjsonresource.cpp#JsonResource::save"
      ],
      "evidence": [
        {
          "path": "app/core/gimpjsonresource.c",
          "revision": "native",
          "anchor": "private->root = owned;",
          "expected": "  private->root = owned;"
        },
        {
          "path": "app/core/gimplayerpreset.c",
          "revision": "native",
          "anchor": "json_generator_set_root (generator, snapshot);",
          "expected": "  json_generator_set_root (generator, snapshot);"
        },
        {
          "path": "app/presets/gimpjsonresource.cpp",
          "revision": "legacy",
          "anchor": "generator [json_generator_set_root] (root);",
          "expected": "  generator [json_generator_set_root] (root);"
        },
        {
          "path": "app/core/gimplayerpreset-apply.cpp",
          "revision": "native",
          "anchor": "const char *name = string (member (object, \"name\"));",
          "expected": "        const char *name = string (member (object, \"name\"));"
        },
        {
          "path": "app/presets/layer-preset.cpp",
          "revision": "legacy",
          "anchor": "const gchar*         name           = node_json.has(\"name\")?",
          "expected": "    const gchar*         name           = node_json.has(\"name\")? (const gchar*)node_json[\"name\"] : NULL;"
        }
      ],
      "assertion": "config.10.preset.replacement-layer[].name.schema",
      "test_file": "migration/tests/test_persistence_config_contract.py",
      "source_keys": [
        [
          "native",
          "object",
          "name"
        ],
        [
          "legacy",
          "node_json",
          "name"
        ]
      ]
    },
    {
      "id": "config.10.preset.replacement-layer[].source",
      "route": "10",
      "disposition": "saved-field",
      "storage": "layer-preset JSON",
      "version": "native GIMP3 source; legacy afa43fae source",
      "saved_name": "replacement-layer[].source",
      "native_type": "JSON string source or beginning # selects current source; other string/null no content source",
      "legacy_type": "JSON string source or beginning # selects current source",
      "owner": "GimpJsonResource private root, inherited by GimpLayerPreset (native); GimpJsonResource.root (legacy)",
      "default": "NULL content source; source target independent.",
      "absence": "Native new normal empty content; new clone may unresolved source. Unknown source string has no content.",
      "flags": "Complete JSON tree saved; only root.name is synchronized from resource name on native save. Apply interprets the paths below; retention does not imply successful application.",
      "reader": [
        "app/core/gimplayerpreset-apply.cpp#gimp_layer_preset_apply_full",
        "app/core/gimplayerpreset.c#gimp_layer_preset_load"
      ],
      "writer": [
        "app/core/gimplayerpreset.c#save"
      ],
      "legacy_reader": [
        "app/presets/layer-preset.cpp#LayerPresetApplier",
        "app/presets/gimpjsonresource.cpp#JsonResource::load"
      ],
      "legacy_writer": [
        "app/presets/gimpjsonresource.cpp#JsonResource::save"
      ],
      "evidence": [
        {
          "path": "app/core/gimpjsonresource.c",
          "revision": "native",
          "anchor": "private->root = owned;",
          "expected": "  private->root = owned;"
        },
        {
          "path": "app/core/gimplayerpreset.c",
          "revision": "native",
          "anchor": "json_generator_set_root (generator, snapshot);",
          "expected": "  json_generator_set_root (generator, snapshot);"
        },
        {
          "path": "app/presets/gimpjsonresource.cpp",
          "revision": "legacy",
          "anchor": "generator [json_generator_set_root] (root);",
          "expected": "  generator [json_generator_set_root] (root);"
        },
        {
          "path": "app/core/gimplayerpreset-apply.cpp",
          "revision": "native",
          "anchor": "const char *content = string (member (object, \"source\"));",
          "expected": "        const char *content = string (member (object, \"source\"));"
        },
        {
          "path": "app/presets/layer-preset.cpp",
          "revision": "legacy",
          "anchor": "content_source = get_source",
          "expected": "    GimpLayer*           content_source = get_source  (node_json[\"source\"],   source); // source, *empty"
        }
      ],
      "assertion": "config.10.preset.replacement-layer[].source.schema",
      "test_file": "migration/tests/test_persistence_config_contract.py",
      "source_keys": [
        [
          "native",
          "object",
          "source"
        ],
        [
          "legacy",
          "node_json",
          "source"
        ]
      ]
    },
    {
      "id": "config.10.preset.replacement-layer[].boundary",
      "route": "10",
      "disposition": "saved-field",
      "storage": "layer-preset JSON",
      "version": "native GIMP3 source; legacy afa43fae source",
      "saved_name": "replacement-layer[].boundary",
      "native_type": "JSON array [x1,y1,x2,y2] of gint, or source/selection/full string",
      "legacy_type": "JSON array four integer coordinates or source/selection/full",
      "owner": "GimpJsonResource private root, inherited by GimpLayerPreset (native); GimpJsonResource.root (legacy)",
      "default": "Absent/null zero bounds; new layer nonpositive dimensions fall back to image width/height. Existing source bounds unchanged when w/h zero.",
      "absence": "Native rejects other string, non-four array, nonintegral/out-of-range or inverted/oversized dimensions. Legacy missing returns all-zero boundary.",
      "flags": "Complete JSON tree saved; only root.name is synchronized from resource name on native save. Apply interprets the paths below; retention does not imply successful application.",
      "reader": [
        "app/core/gimplayerpreset-apply.cpp#gimp_layer_preset_apply_full",
        "app/core/gimplayerpreset.c#gimp_layer_preset_load"
      ],
      "writer": [
        "app/core/gimplayerpreset.c#save"
      ],
      "legacy_reader": [
        "app/presets/layer-preset.cpp#LayerPresetApplier",
        "app/presets/gimpjsonresource.cpp#JsonResource::load"
      ],
      "legacy_writer": [
        "app/presets/gimpjsonresource.cpp#JsonResource::save"
      ],
      "evidence": [
        {
          "path": "app/core/gimpjsonresource.c",
          "revision": "native",
          "anchor": "private->root = owned;",
          "expected": "  private->root = owned;"
        },
        {
          "path": "app/core/gimplayerpreset.c",
          "revision": "native",
          "anchor": "json_generator_set_root (generator, snapshot);",
          "expected": "  json_generator_set_root (generator, snapshot);"
        },
        {
          "path": "app/presets/gimpjsonresource.cpp",
          "revision": "legacy",
          "anchor": "generator [json_generator_set_root] (root);",
          "expected": "  generator [json_generator_set_root] (root);"
        },
        {
          "path": "app/core/gimplayerpreset-apply.cpp",
          "revision": "native",
          "anchor": "p.bounds = bounds (member (object, \"boundary\"), image, source);",
          "expected": "        Planned p; p.parent = parent; p.index = index; p.bounds = bounds (member (object, \"boundary\"), image, source);"
        },
        {
          "path": "app/presets/layer-preset.cpp",
          "revision": "legacy",
          "anchor": "Boundary             boundary       = get_boundary",
          "expected": "    Boundary             boundary       = get_boundary(node_json[\"boundary\"], source); // source, selection, full, [*,*,*,*]"
        }
      ],
      "assertion": "config.10.preset.replacement-layer[].boundary.schema",
      "test_file": "migration/tests/test_persistence_config_contract.py",
      "source_keys": [
        [
          "native",
          "object",
          "boundary"
        ],
        [
          "legacy",
          "node_json",
          "boundary"
        ]
      ]
    },
    {
      "id": "config.10.preset.replacement-layer[].mode",
      "route": "10",
      "disposition": "saved-field",
      "storage": "layer-preset JSON",
      "version": "native GIMP3 source; legacy afa43fae source",
      "saved_name": "replacement-layer[].mode",
      "native_type": "JSON integral legacy mode or exact legacy nick string or source",
      "legacy_type": "JSON numeric GimpLayerModeEffects or enum nick or source",
      "owner": "GimpJsonResource private root, inherited by GimpLayerPreset (native); GimpJsonResource.root (legacy)",
      "default": "Missing/null/unrecognized nick gives -1 sentinel; source target unchanged; new filter Painter replace and other new normal.",
      "absence": "Native numeric maps by explicit legacy map; unknown numeric fails. Uppercase Multiply is not a recognized nick.",
      "flags": "Complete JSON tree saved; only root.name is synchronized from resource name on native save. Apply interprets the paths below; retention does not imply successful application.",
      "reader": [
        "app/core/gimplayerpreset-apply.cpp#gimp_layer_preset_apply_full",
        "app/core/gimplayerpreset.c#gimp_layer_preset_load"
      ],
      "writer": [
        "app/core/gimplayerpreset.c#save"
      ],
      "legacy_reader": [
        "app/presets/layer-preset.cpp#LayerPresetApplier",
        "app/presets/gimpjsonresource.cpp#JsonResource::load"
      ],
      "legacy_writer": [
        "app/presets/gimpjsonresource.cpp#JsonResource::save"
      ],
      "evidence": [
        {
          "path": "app/core/gimpjsonresource.c",
          "revision": "native",
          "anchor": "private->root = owned;",
          "expected": "  private->root = owned;"
        },
        {
          "path": "app/core/gimplayerpreset.c",
          "revision": "native",
          "anchor": "json_generator_set_root (generator, snapshot);",
          "expected": "  json_generator_set_root (generator, snapshot);"
        },
        {
          "path": "app/presets/gimpjsonresource.cpp",
          "revision": "legacy",
          "anchor": "generator [json_generator_set_root] (root);",
          "expected": "  generator [json_generator_set_root] (root);"
        },
        {
          "path": "app/core/gimplayerpreset-apply.cpp",
          "revision": "native",
          "anchor": "p.mode = mode (member (object, \"mode\"), source);",
          "expected": "        p.mode = mode (member (object, \"mode\"), source);"
        },
        {
          "path": "app/presets/layer-preset.cpp",
          "revision": "legacy",
          "anchor": "GimpLayerModeEffects mode           = get_mode",
          "expected": "    GimpLayerModeEffects mode           = get_mode    (node_json[\"mode\"],     source); // source or layer mode"
        }
      ],
      "assertion": "config.10.preset.replacement-layer[].mode.schema",
      "test_file": "migration/tests/test_persistence_config_contract.py",
      "source_keys": [
        [
          "native",
          "object",
          "mode"
        ],
        [
          "legacy",
          "node_json",
          "mode"
        ]
      ]
    },
    {
      "id": "config.10.preset.replacement-layer[].opacity",
      "route": "10",
      "disposition": "saved-field",
      "storage": "layer-preset JSON",
      "version": "native GIMP3 source; legacy afa43fae source",
      "saved_name": "replacement-layer[].opacity",
      "native_type": "JSON finite number or source string",
      "legacy_type": "JSON number or source string",
      "owner": "GimpJsonResource private root, inherited by GimpLayerPreset (native); GimpJsonResource.root (legacy)",
      "default": "Absent/null -1 sentinel; new layer invalid/outside0..1 becomes1; source target applied only within0..1.",
      "absence": "No source mutation for absent opacity.",
      "flags": "Complete JSON tree saved; only root.name is synchronized from resource name on native save. Apply interprets the paths below; retention does not imply successful application.",
      "reader": [
        "app/core/gimplayerpreset-apply.cpp#gimp_layer_preset_apply_full",
        "app/core/gimplayerpreset.c#gimp_layer_preset_load"
      ],
      "writer": [
        "app/core/gimplayerpreset.c#save"
      ],
      "legacy_reader": [
        "app/presets/layer-preset.cpp#LayerPresetApplier",
        "app/presets/gimpjsonresource.cpp#JsonResource::load"
      ],
      "legacy_writer": [
        "app/presets/gimpjsonresource.cpp#JsonResource::save"
      ],
      "evidence": [
        {
          "path": "app/core/gimpjsonresource.c",
          "revision": "native",
          "anchor": "private->root = owned;",
          "expected": "  private->root = owned;"
        },
        {
          "path": "app/core/gimplayerpreset.c",
          "revision": "native",
          "anchor": "json_generator_set_root (generator, snapshot);",
          "expected": "  json_generator_set_root (generator, snapshot);"
        },
        {
          "path": "app/presets/gimpjsonresource.cpp",
          "revision": "legacy",
          "anchor": "generator [json_generator_set_root] (root);",
          "expected": "  generator [json_generator_set_root] (root);"
        },
        {
          "path": "app/core/gimplayerpreset-apply.cpp",
          "revision": "native",
          "anchor": "JsonNode *opacity = member (object, \"opacity\");",
          "expected": "        JsonNode *opacity = member (object, \"opacity\");"
        },
        {
          "path": "app/presets/layer-preset.cpp",
          "revision": "legacy",
          "anchor": "double               opacity        = get_opacity",
          "expected": "    double               opacity        = get_opacity (node_json[\"opacity\"],  source); // source, double, default(none)"
        }
      ],
      "assertion": "config.10.preset.replacement-layer[].opacity.schema",
      "test_file": "migration/tests/test_persistence_config_contract.py",
      "source_keys": [
        [
          "native",
          "object",
          "opacity"
        ],
        [
          "legacy",
          "node_json",
          "opacity"
        ]
      ]
    },
    {
      "id": "config.10.preset.replacement-layer[].alpha",
      "route": "10",
      "disposition": "saved-field",
      "storage": "layer-preset JSON",
      "version": "native GIMP3 source; legacy afa43fae source",
      "saved_name": "replacement-layer[].alpha",
      "native_type": "Opaque JSON retained; native replacement applier does not read alpha",
      "legacy_type": "JSON boolean or source string",
      "owner": "GimpJsonResource private root, inherited by GimpLayerPreset (native); GimpJsonResource.root (legacy)",
      "default": "Legacy absent/null copies source has-alpha; non-value legacy returns true. Native new normal creation requests alpha TRUE regardless of this retained field.",
      "absence": "Native preserves value without application; legacy reads source alpha default.",
      "flags": "Complete JSON tree saved; only root.name is synchronized from resource name on native save. Apply interprets the paths below; retention does not imply successful application.",
      "reader": [
        "app/core/gimplayerpreset-apply.cpp#gimp_layer_preset_apply_full",
        "app/core/gimplayerpreset.c#gimp_layer_preset_load"
      ],
      "writer": [
        "app/core/gimplayerpreset.c#save"
      ],
      "legacy_reader": [
        "app/presets/layer-preset.cpp#LayerPresetApplier",
        "app/presets/gimpjsonresource.cpp#JsonResource::load"
      ],
      "legacy_writer": [
        "app/presets/gimpjsonresource.cpp#JsonResource::save"
      ],
      "evidence": [
        {
          "path": "app/core/gimpjsonresource.c",
          "revision": "native",
          "anchor": "private->root = owned;",
          "expected": "  private->root = owned;"
        },
        {
          "path": "app/core/gimplayerpreset.c",
          "revision": "native",
          "anchor": "json_generator_set_root (generator, snapshot);",
          "expected": "  json_generator_set_root (generator, snapshot);"
        },
        {
          "path": "app/presets/gimpjsonresource.cpp",
          "revision": "legacy",
          "anchor": "generator [json_generator_set_root] (root);",
          "expected": "  generator [json_generator_set_root] (root);"
        },
        {
          "path": "app/core/gimplayerpreset-apply.cpp",
          "revision": "native",
          "anchor": "gimp_image_get_layer_format (image, TRUE)",
          "expected": "                gimp_layer_new (image, w, h, gimp_image_get_layer_format (image, TRUE), name ? name : _(\"New Layer\"), p.opacity, p.mode);"
        },
        {
          "path": "app/presets/layer-preset.cpp",
          "revision": "legacy",
          "anchor": "bool                 alpha          = get_alpha",
          "expected": "    bool                 alpha          = get_alpha   (node_json[\"alpha\"],    source); // *source, true, false"
        }
      ],
      "assertion": "config.10.preset.replacement-layer[].alpha.schema",
      "test_file": "migration/tests/test_persistence_config_contract.py",
      "source_keys": [
        [
          "legacy",
          "node_json",
          "alpha"
        ]
      ]
    },
    {
      "id": "config.10.preset.replacement-layer[].children",
      "route": "10",
      "disposition": "saved-field",
      "storage": "layer-preset JSON",
      "version": "native GIMP3 source; legacy afa43fae source",
      "saved_name": "replacement-layer[].children",
      "native_type": "JSON array of recursive layer objects",
      "legacy_type": "JSON array of recursive layer objects",
      "owner": "GimpJsonResource private root, inherited by GimpLayerPreset (native); GimpJsonResource.root (legacy)",
      "default": "No children when absent; only newly created group recurses.",
      "absence": "Native null/non-array if present fails; source target group does not recurse.",
      "flags": "Complete JSON tree saved; only root.name is synchronized from resource name on native save. Apply interprets the paths below; retention does not imply successful application.",
      "reader": [
        "app/core/gimplayerpreset-apply.cpp#gimp_layer_preset_apply_full",
        "app/core/gimplayerpreset.c#gimp_layer_preset_load"
      ],
      "writer": [
        "app/core/gimplayerpreset.c#save"
      ],
      "legacy_reader": [
        "app/presets/layer-preset.cpp#LayerPresetApplier",
        "app/presets/gimpjsonresource.cpp#JsonResource::load"
      ],
      "legacy_writer": [
        "app/presets/gimpjsonresource.cpp#JsonResource::save"
      ],
      "evidence": [
        {
          "path": "app/core/gimpjsonresource.c",
          "revision": "native",
          "anchor": "private->root = owned;",
          "expected": "  private->root = owned;"
        },
        {
          "path": "app/core/gimplayerpreset.c",
          "revision": "native",
          "anchor": "json_generator_set_root (generator, snapshot);",
          "expected": "  json_generator_set_root (generator, snapshot);"
        },
        {
          "path": "app/presets/gimpjsonresource.cpp",
          "revision": "legacy",
          "anchor": "generator [json_generator_set_root] (root);",
          "expected": "  generator [json_generator_set_root] (root);"
        },
        {
          "path": "app/core/gimplayerpreset-apply.cpp",
          "revision": "native",
          "anchor": "JsonNode *children = member (object, \"children\");",
          "expected": "            JsonNode *children = member (object, \"children\");"
        },
        {
          "path": "app/presets/layer-preset.cpp",
          "revision": "legacy",
          "anchor": "auto children_json = node_json[\"children\"];",
          "expected": "        auto children_json = node_json[\"children\"];"
        }
      ],
      "assertion": "config.10.preset.replacement-layer[].children.schema",
      "test_file": "migration/tests/test_persistence_config_contract.py",
      "source_keys": [
        [
          "native",
          "object",
          "children"
        ],
        [
          "legacy",
          "node_json",
          "children"
        ],
        [
          "legacy",
          "json",
          "children"
        ]
      ]
    },
    {
      "id": "config.10.preset.replacement-layer[].filter",
      "route": "10",
      "disposition": "saved-field",
      "storage": "layer-preset JSON",
      "version": "native GIMP3 source; legacy afa43fae source",
      "saved_name": "replacement-layer[].filter",
      "native_type": "JSON object",
      "legacy_type": "JSON object",
      "owner": "GimpJsonResource private root, inherited by GimpLayerPreset (native); GimpJsonResource.root (legacy)",
      "default": "No default definition; required for new filter type.",
      "absence": "Native fails when missing/non-object.",
      "flags": "Complete JSON tree saved; only root.name is synchronized from resource name on native save. Apply interprets the paths below; retention does not imply successful application.",
      "reader": [
        "app/core/gimplayerpreset-apply.cpp#gimp_layer_preset_apply_full",
        "app/core/gimplayerpreset.c#gimp_layer_preset_load"
      ],
      "writer": [
        "app/core/gimplayerpreset.c#save"
      ],
      "legacy_reader": [
        "app/presets/layer-preset.cpp#LayerPresetApplier",
        "app/presets/gimpjsonresource.cpp#JsonResource::load"
      ],
      "legacy_writer": [
        "app/presets/gimpjsonresource.cpp#JsonResource::save"
      ],
      "evidence": [
        {
          "path": "app/core/gimpjsonresource.c",
          "revision": "native",
          "anchor": "private->root = owned;",
          "expected": "  private->root = owned;"
        },
        {
          "path": "app/core/gimplayerpreset.c",
          "revision": "native",
          "anchor": "json_generator_set_root (generator, snapshot);",
          "expected": "  json_generator_set_root (generator, snapshot);"
        },
        {
          "path": "app/presets/gimpjsonresource.cpp",
          "revision": "legacy",
          "anchor": "generator [json_generator_set_root] (root);",
          "expected": "  generator [json_generator_set_root] (root);"
        },
        {
          "path": "app/core/gimplayerpreset-apply.cpp",
          "revision": "native",
          "anchor": "install_filter (layer, member (object, \"filter\"));",
          "expected": "                install_filter (layer, member (object, \"filter\"));"
        },
        {
          "path": "app/presets/layer-preset.cpp",
          "revision": "legacy",
          "anchor": "auto filter_json       = node_json[\"filter\"];",
          "expected": "        auto filter_json       = node_json[\"filter\"];"
        }
      ],
      "assertion": "config.10.preset.replacement-layer[].filter.schema",
      "test_file": "migration/tests/test_persistence_config_contract.py",
      "source_keys": [
        [
          "native",
          "object",
          "filter"
        ],
        [
          "legacy",
          "node_json",
          "filter"
        ]
      ]
    },
    {
      "id": "config.10.preset.replacement-layer[].filter.name",
      "route": "10",
      "disposition": "saved-field",
      "storage": "layer-preset JSON",
      "version": "native GIMP3 source; legacy afa43fae source",
      "saved_name": "replacement-layer[].filter.name",
      "native_type": "JSON nonempty string",
      "legacy_type": "JSON procedure string",
      "owner": "GimpJsonResource private root, inherited by GimpLayerPreset (native); GimpJsonResource.root (legacy)",
      "default": "No default; native empty/missing fails.",
      "absence": "Legacy passes requested procedure name to filter layer; native preserves definition for supported exact executors.",
      "flags": "Complete JSON tree saved; only root.name is synchronized from resource name on native save. Apply interprets the paths below; retention does not imply successful application.",
      "reader": [
        "app/core/gimplayerpreset-apply.cpp#gimp_layer_preset_apply_full",
        "app/core/gimplayerpreset.c#gimp_layer_preset_load"
      ],
      "writer": [
        "app/core/gimplayerpreset.c#save"
      ],
      "legacy_reader": [
        "app/presets/layer-preset.cpp#LayerPresetApplier",
        "app/presets/gimpjsonresource.cpp#JsonResource::load"
      ],
      "legacy_writer": [
        "app/presets/gimpjsonresource.cpp#JsonResource::save"
      ],
      "evidence": [
        {
          "path": "app/core/gimpjsonresource.c",
          "revision": "native",
          "anchor": "private->root = owned;",
          "expected": "  private->root = owned;"
        },
        {
          "path": "app/core/gimplayerpreset.c",
          "revision": "native",
          "anchor": "json_generator_set_root (generator, snapshot);",
          "expected": "  json_generator_set_root (generator, snapshot);"
        },
        {
          "path": "app/presets/gimpjsonresource.cpp",
          "revision": "legacy",
          "anchor": "generator [json_generator_set_root] (root);",
          "expected": "  generator [json_generator_set_root] (root);"
        },
        {
          "path": "app/core/gimplayerpreset-apply.cpp",
          "revision": "native",
          "anchor": "const char *procedure = string (member (filter, \"name\"));",
          "expected": "  const char *procedure = string (member (filter, \"name\"));"
        },
        {
          "path": "app/presets/layer-preset.cpp",
          "revision": "legacy",
          "anchor": "const gchar* proc_name = filter_json[\"name\"];",
          "expected": "        const gchar* proc_name = filter_json[\"name\"];"
        }
      ],
      "assertion": "config.10.preset.replacement-layer[].filter.name.schema",
      "test_file": "migration/tests/test_persistence_config_contract.py",
      "source_keys": [
        [
          "native",
          "filter",
          "name"
        ],
        [
          "legacy",
          "filter_json",
          "name"
        ]
      ]
    },
    {
      "id": "config.10.preset.replacement-layer[].filter.args",
      "route": "10",
      "disposition": "saved-field",
      "storage": "layer-preset JSON",
      "version": "native GIMP3 source; legacy afa43fae source",
      "saved_name": "replacement-layer[].filter.args",
      "native_type": "JSON array, maximum65536 entries",
      "legacy_type": "JSON array",
      "owner": "GimpJsonResource private root, inherited by GimpLayerPreset (native); GimpJsonResource.root (legacy)",
      "default": "No default; native requires array.",
      "absence": "Native missing/non-array fails; unknown argument types retained as null typed-value slots.",
      "flags": "Complete JSON tree saved; only root.name is synchronized from resource name on native save. Apply interprets the paths below; retention does not imply successful application.",
      "reader": [
        "app/core/gimplayerpreset-apply.cpp#gimp_layer_preset_apply_full",
        "app/core/gimplayerpreset.c#gimp_layer_preset_load"
      ],
      "writer": [
        "app/core/gimplayerpreset.c#save"
      ],
      "legacy_reader": [
        "app/presets/layer-preset.cpp#LayerPresetApplier",
        "app/presets/gimpjsonresource.cpp#JsonResource::load"
      ],
      "legacy_writer": [
        "app/presets/gimpjsonresource.cpp#JsonResource::save"
      ],
      "evidence": [
        {
          "path": "app/core/gimpjsonresource.c",
          "revision": "native",
          "anchor": "private->root = owned;",
          "expected": "  private->root = owned;"
        },
        {
          "path": "app/core/gimplayerpreset.c",
          "revision": "native",
          "anchor": "json_generator_set_root (generator, snapshot);",
          "expected": "  json_generator_set_root (generator, snapshot);"
        },
        {
          "path": "app/presets/gimpjsonresource.cpp",
          "revision": "legacy",
          "anchor": "generator [json_generator_set_root] (root);",
          "expected": "  generator [json_generator_set_root] (root);"
        },
        {
          "path": "app/core/gimplayerpreset-apply.cpp",
          "revision": "native",
          "anchor": "JsonNode *arguments = member (filter, \"args\");",
          "expected": "  JsonNode *arguments = member (filter, \"args\");"
        },
        {
          "path": "app/core/gimplayerpreset-apply.cpp",
          "revision": "native",
          "anchor": "if (count > 65536)",
          "expected": "  if (count > 65536) fail (\"Too many filter arguments\");"
        },
        {
          "path": "app/presets/layer-preset.cpp",
          "revision": "legacy",
          "anchor": "auto proc_args = hold( get_args(filter_json[\"args\"], source) );",
          "expected": "        auto proc_args = hold( get_args(filter_json[\"args\"], source) );"
        }
      ],
      "assertion": "config.10.preset.replacement-layer[].filter.args.schema",
      "test_file": "migration/tests/test_persistence_config_contract.py",
      "source_keys": [
        [
          "native",
          "filter",
          "args"
        ],
        [
          "legacy",
          "filter_json",
          "args"
        ]
      ]
    },
    {
      "id": "config.10.preset.replacement-layer[].filter.args[].type",
      "route": "10",
      "disposition": "saved-field",
      "storage": "layer-preset JSON",
      "version": "native GIMP3 source; legacy afa43fae source",
      "saved_name": "replacement-layer[].filter.args[].type",
      "native_type": "JSON string FLOAT/INT32/INT8/STRING or opaque unknown",
      "legacy_type": "JSON string FLOAT/INT32/INT8/STRING/RGB/INT8ARRAY or unknown",
      "owner": "GimpJsonResource private root, inherited by GimpLayerPreset (native); GimpJsonResource.root (legacy)",
      "default": "Native missing/null fallback empty string; unknown types produce uninitialized/null GValue slots.",
      "absence": "First3 Edge/Gauss argument positions become integer0 placeholders; arbitrary JSON still retained.",
      "flags": "Complete JSON tree saved; only root.name is synchronized from resource name on native save. Apply interprets the paths below; retention does not imply successful application.",
      "reader": [
        "app/core/gimplayerpreset-apply.cpp#gimp_layer_preset_apply_full",
        "app/core/gimplayerpreset.c#gimp_layer_preset_load"
      ],
      "writer": [
        "app/core/gimplayerpreset.c#save"
      ],
      "legacy_reader": [
        "app/presets/layer-preset.cpp#LayerPresetApplier",
        "app/presets/gimpjsonresource.cpp#JsonResource::load"
      ],
      "legacy_writer": [
        "app/presets/gimpjsonresource.cpp#JsonResource::save"
      ],
      "evidence": [
        {
          "path": "app/core/gimpjsonresource.c",
          "revision": "native",
          "anchor": "private->root = owned;",
          "expected": "  private->root = owned;"
        },
        {
          "path": "app/core/gimplayerpreset.c",
          "revision": "native",
          "anchor": "json_generator_set_root (generator, snapshot);",
          "expected": "  json_generator_set_root (generator, snapshot);"
        },
        {
          "path": "app/presets/gimpjsonresource.cpp",
          "revision": "legacy",
          "anchor": "generator [json_generator_set_root] (root);",
          "expected": "  generator [json_generator_set_root] (root);"
        },
        {
          "path": "app/core/gimplayerpreset-apply.cpp",
          "revision": "native",
          "anchor": "const char *type = string (member (arg, \"type\"), \"\");",
          "expected": "      const char *type = string (member (arg, \"type\"), \"\");"
        },
        {
          "path": "app/core/gimplayerpreset-apply.cpp",
          "revision": "native",
          "anchor": "if (i < 3 &&",
          "expected": "      if (i < 3 && (equal (procedure, \"plug-in-edge\") || equal (procedure, \"plug-in-gauss\"))) { g_value_init (&v, G_TYPE_INT); g_value_set_int (&v, 0); }"
        },
        {
          "path": "app/presets/layer-preset.cpp",
          "revision": "legacy",
          "anchor": "const gchar* type = arg_json[\"type\"];",
          "expected": "        const gchar* type = arg_json[\"type\"];"
        }
      ],
      "assertion": "config.10.preset.replacement-layer[].filter.args[].type.schema",
      "test_file": "migration/tests/test_persistence_config_contract.py",
      "source_keys": [
        [
          "native",
          "arg",
          "type"
        ],
        [
          "legacy",
          "arg_json",
          "type"
        ]
      ]
    },
    {
      "id": "config.10.preset.replacement-layer[].filter.args[].value",
      "route": "10",
      "disposition": "saved-field",
      "storage": "layer-preset JSON",
      "version": "native GIMP3 source; legacy afa43fae source",
      "saved_name": "replacement-layer[].filter.args[].value",
      "native_type": "JSON number/string/null/arbitrary retained value; FLOAT narrows to float then double, INT32 gint, INT8 guint cast, STRING UTF8",
      "legacy_type": "JSON value mapped to GValue FLOAT->double after float, INT32->gint, INT8->guint, STRING->gchar*; RGB/INT8ARRAY unsupported",
      "owner": "GimpJsonResource private root, inherited by GimpLayerPreset (native); GimpJsonResource.root (legacy)",
      "default": "No default for numeric recognized type; missing string becomes NULL; opaque/unknown creates null typed slot.",
      "absence": "Native numeric missing/type/range/nonfinite failures; JSON preservation includes unsupported values.",
      "flags": "Complete JSON tree saved; only root.name is synchronized from resource name on native save. Apply interprets the paths below; retention does not imply successful application.",
      "reader": [
        "app/core/gimplayerpreset-apply.cpp#gimp_layer_preset_apply_full",
        "app/core/gimplayerpreset.c#gimp_layer_preset_load"
      ],
      "writer": [
        "app/core/gimplayerpreset.c#save"
      ],
      "legacy_reader": [
        "app/presets/layer-preset.cpp#LayerPresetApplier",
        "app/presets/gimpjsonresource.cpp#JsonResource::load"
      ],
      "legacy_writer": [
        "app/presets/gimpjsonresource.cpp#JsonResource::save"
      ],
      "evidence": [
        {
          "path": "app/core/gimpjsonresource.c",
          "revision": "native",
          "anchor": "private->root = owned;",
          "expected": "  private->root = owned;"
        },
        {
          "path": "app/core/gimplayerpreset.c",
          "revision": "native",
          "anchor": "json_generator_set_root (generator, snapshot);",
          "expected": "  json_generator_set_root (generator, snapshot);"
        },
        {
          "path": "app/presets/gimpjsonresource.cpp",
          "revision": "legacy",
          "anchor": "generator [json_generator_set_root] (root);",
          "expected": "  generator [json_generator_set_root] (root);"
        },
        {
          "path": "app/core/gimplayerpreset-apply.cpp",
          "revision": "native",
          "anchor": "JsonNode *value = member (arg, \"value\");",
          "expected": "      JsonNode *value = member (arg, \"value\");"
        },
        {
          "path": "app/core/gimplayerpreset-apply.cpp",
          "revision": "native",
          "anchor": "else if (equal (type, \"FLOAT\"))",
          "expected": "      else if (equal (type, \"FLOAT\")) { float f = number (value); if (!std::isfinite (f)) fail (\"Filter float is out of range\"); g_value_init (&v, G_TYPE_DOUBLE); g_value_set_double (&v, f); }"
        },
        {
          "path": "app/presets/layer-preset.cpp",
          "revision": "legacy",
          "anchor": "float v = (float)(double)arg_json[\"value\"];",
          "expected": "          float v = (float)(double)arg_json[\"value\"];"
        }
      ],
      "assertion": "config.10.preset.replacement-layer[].filter.args[].value.schema",
      "test_file": "migration/tests/test_persistence_config_contract.py",
      "source_keys": [
        [
          "native",
          "arg",
          "value"
        ],
        [
          "legacy",
          "arg_json",
          "value"
        ]
      ]
    },
    {
      "id": "config.06.parasite.name",
      "route": "06",
      "disposition": "saved-field",
      "storage": "GimpConfig S-expression",
      "version": "native GIMP3 source; legacy afa43fae source",
      "saved_name": "parasite[].name",
      "native_type": "UTF8 quoted string",
      "legacy_type": "UTF8 quoted string",
      "owner": "GimpParasite owned by GimpParasiteList",
      "default": "Required name; no record default.",
      "absence": "Missing name prevents construction.",
      "flags": "Only gimp_parasite_is_persistent(parasite); global parasiterc independent of XCF.",
      "reader": [
        "app/core/gimpparasitelist.c#gimp_parasite_list_deserialize",
        "app/core/gimp-parasites.c#gimp_parasiterc_load"
      ],
      "writer": [
        "app/core/gimpparasitelist.c#parasite_serialize",
        "app/core/gimp-parasites.c#gimp_parasiterc_save"
      ],
      "legacy_reader": [
        "app/core/gimpparasitelist.c#gimp_parasite_list_deserialize",
        "app/core/gimp-parasites.c#gimp_parasiterc_load"
      ],
      "legacy_writer": [
        "app/core/gimpparasitelist.c#parasite_serialize",
        "app/core/gimp-parasites.c#gimp_parasiterc_save"
      ],
      "evidence": [
        {
          "path": "app/core/gimpparasitelist.c",
          "revision": "native",
          "anchor": "gimp_parasite_get_name (parasite)",
          "expected": "                             gimp_parasite_get_name (parasite),"
        },
        {
          "path": "app/core/gimpparasitelist.c",
          "revision": "legacy",
          "anchor": "gimp_parasite_name (parasite)",
          "expected": "                             gimp_parasite_name (parasite),"
        }
      ],
      "assertion": "config.06.parasite.name.schema",
      "test_file": "migration/tests/test_persistence_config_contract.py"
    },
    {
      "id": "config.06.parasite.flags",
      "route": "06",
      "disposition": "saved-field",
      "storage": "GimpConfig S-expression",
      "version": "native GIMP3 source; legacy afa43fae source",
      "saved_name": "parasite[].flags",
      "native_type": "unsigned-long textual mask; GimpParasite guint32 flags",
      "legacy_type": "unsigned-long textual mask; GimpParasite guint32 flags",
      "owner": "GimpParasite owned by GimpParasiteList",
      "default": "Parser temporary gint0 is not an absent-field default.",
      "absence": "Missing flag integer prevents construction.",
      "flags": "Only gimp_parasite_is_persistent(parasite); global parasiterc independent of XCF.",
      "reader": [
        "app/core/gimpparasitelist.c#gimp_parasite_list_deserialize",
        "app/core/gimp-parasites.c#gimp_parasiterc_load"
      ],
      "writer": [
        "app/core/gimpparasitelist.c#parasite_serialize",
        "app/core/gimp-parasites.c#gimp_parasiterc_save"
      ],
      "legacy_reader": [
        "app/core/gimpparasitelist.c#gimp_parasite_list_deserialize",
        "app/core/gimp-parasites.c#gimp_parasiterc_load"
      ],
      "legacy_writer": [
        "app/core/gimpparasitelist.c#parasite_serialize",
        "app/core/gimp-parasites.c#gimp_parasiterc_save"
      ],
      "evidence": [
        {
          "path": "app/core/gimpparasitelist.c",
          "revision": "native",
          "anchor": "gimp_parasite_get_flags (parasite)",
          "expected": "                             gimp_parasite_get_flags (parasite),"
        },
        {
          "path": "app/core/gimpparasitelist.c",
          "revision": "legacy",
          "anchor": "gimp_parasite_flags (parasite)",
          "expected": "                             gimp_parasite_flags (parasite),"
        }
      ],
      "assertion": "config.06.parasite.flags.schema",
      "test_file": "migration/tests/test_persistence_config_contract.py"
    },
    {
      "id": "config.06.parasite.size",
      "route": "06",
      "disposition": "saved-field",
      "storage": "GimpConfig S-expression",
      "version": "native GIMP3 source; legacy afa43fae source",
      "saved_name": "parasite[].size",
      "native_type": "textual guint32 byte length; parser gint",
      "legacy_type": "textual guint32 byte length; parser gint",
      "owner": "GimpParasite owned by GimpParasiteList",
      "default": "Parser temporary0; old plain-string format derives strlen.",
      "absence": "Missing numeric size is accepted only if followed by old-format string.",
      "flags": "Only gimp_parasite_is_persistent(parasite); global parasiterc independent of XCF.",
      "reader": [
        "app/core/gimpparasitelist.c#gimp_parasite_list_deserialize",
        "app/core/gimp-parasites.c#gimp_parasiterc_load"
      ],
      "writer": [
        "app/core/gimpparasitelist.c#parasite_serialize",
        "app/core/gimp-parasites.c#gimp_parasiterc_save"
      ],
      "legacy_reader": [
        "app/core/gimpparasitelist.c#gimp_parasite_list_deserialize",
        "app/core/gimp-parasites.c#gimp_parasiterc_load"
      ],
      "legacy_writer": [
        "app/core/gimpparasitelist.c#parasite_serialize",
        "app/core/gimp-parasites.c#gimp_parasiterc_save"
      ],
      "evidence": [
        {
          "path": "app/core/gimpparasitelist.c",
          "revision": "native",
          "anchor": "(long unsigned int) parasite_size",
          "expected": "                             (long unsigned int) parasite_size);"
        },
        {
          "path": "app/core/gimpparasitelist.c",
          "revision": "legacy",
          "anchor": "parasite_serialize (const gchar",
          "expected": "parasite_serialize (const gchar      *key,"
        }
      ],
      "assertion": "config.06.parasite.size.schema",
      "test_file": "migration/tests/test_persistence_config_contract.py"
    },
    {
      "id": "config.06.parasite.data",
      "route": "06",
      "disposition": "saved-field",
      "storage": "GimpConfig S-expression",
      "version": "native GIMP3 source; legacy afa43fae source",
      "saved_name": "parasite[].data",
      "native_type": "opaque byte sequence encoded by GimpConfigWriter; old quoted string accepted",
      "legacy_type": "opaque byte sequence encoded by GimpConfigWriter; old quoted string accepted",
      "owner": "GimpParasite owned by GimpParasiteList",
      "default": "No record when absent; data length determined by size or old string strlen.",
      "absence": "Missing data token invalidates record.",
      "flags": "Only gimp_parasite_is_persistent(parasite); global parasiterc independent of XCF.",
      "reader": [
        "app/core/gimpparasitelist.c#gimp_parasite_list_deserialize",
        "app/core/gimp-parasites.c#gimp_parasiterc_load"
      ],
      "writer": [
        "app/core/gimpparasitelist.c#parasite_serialize",
        "app/core/gimp-parasites.c#gimp_parasiterc_save"
      ],
      "legacy_reader": [
        "app/core/gimpparasitelist.c#gimp_parasite_list_deserialize",
        "app/core/gimp-parasites.c#gimp_parasiterc_load"
      ],
      "legacy_writer": [
        "app/core/gimpparasitelist.c#parasite_serialize",
        "app/core/gimp-parasites.c#gimp_parasiterc_save"
      ],
      "evidence": [
        {
          "path": "app/core/gimpparasitelist.c",
          "revision": "native",
          "anchor": "gimp_config_writer_data (writer, parasite_size, parasite_contents);",
          "expected": "  gimp_config_writer_data (writer, parasite_size, parasite_contents);"
        },
        {
          "path": "app/core/gimpparasitelist.c",
          "revision": "legacy",
          "anchor": "parasite_serialize (const gchar",
          "expected": "parasite_serialize (const gchar      *key,"
        }
      ],
      "assertion": "config.06.parasite.data.schema",
      "test_file": "migration/tests/test_persistence_config_contract.py"
    },
    {
      "id": "config.06.parasite.records",
      "route": "06",
      "disposition": "dynamic-schema",
      "storage": "GimpConfig S-expression",
      "version": "native GIMP3 source; legacy afa43fae source",
      "saved_name": "parasite[]",
      "native_type": "name-keyed repeated records",
      "legacy_type": "name-keyed repeated records",
      "owner": "GimpParasiteList",
      "default": "New list table NULL, no records.",
      "absence": "Absent file/list leaves empty or existing list; adding same name replaces previous record.",
      "flags": "Only persistent parasites emitted; arbitrary names and payloads preserved.",
      "reader": [
        "app/core/gimpparasitelist.c#gimp_parasite_list_deserialize"
      ],
      "writer": [
        "app/core/gimpparasitelist.c#gimp_parasite_list_serialize"
      ],
      "legacy_reader": [
        "app/core/gimpparasitelist.c#gimp_parasite_list_deserialize"
      ],
      "legacy_writer": [
        "app/core/gimpparasitelist.c#gimp_parasite_list_serialize"
      ],
      "evidence": [
        {
          "path": "app/core/gimpparasitelist.c",
          "revision": "native",
          "anchor": "list->table = NULL;",
          "expected": "      list->table = NULL;"
        },
        {
          "path": "app/core/gimpparasitelist.c",
          "revision": "native",
          "anchor": "gimp_parasite_list_remove (list, parasite->name);",
          "expected": "  gimp_parasite_list_remove (list, parasite->name);"
        },
        {
          "path": "app/core/gimpparasitelist.c",
          "revision": "legacy",
          "anchor": "list->table = NULL;",
          "expected": "      list->table = NULL;"
        }
      ],
      "assertion": "config.06.parasite.records.schema",
      "test_file": "migration/tests/test_persistence_config_contract.py"
    },
    {
      "id": "config.15.toolrc.file-version",
      "route": "15",
      "disposition": "saved-field",
      "storage": "GimpConfig S-expression",
      "version": "native GIMP3 source; legacy afa43fae source",
      "saved_name": "file-version",
      "native_type": "gint text",
      "legacy_type": "gint text",
      "owner": "GimpContainer<GimpToolItem> owned by Gimp.tool_item_list",
      "default": "Writer1; parser initial0; native accepts1 or migrated marker; old reader accepts1.",
      "absence": "Missing/unsupported rejects toolrc.",
      "flags": "Toolrc header and list emitted when writer exists; properties inherited from GimpToolItem/GimpToolGroup.",
      "reader": [
        "app/tools/gimp-tools.c#gimp_tools_deserialize",
        "app/core/gimpcontainer.c#gimp_container_deserialize"
      ],
      "writer": [
        "app/tools/gimp-tools.c#gimp_tools_serialize",
        "app/core/gimpcontainer.c#gimp_container_serialize"
      ],
      "legacy_reader": [
        "app/tools/gimp-tools.c#gimp_tools_deserialize",
        "app/core/gimpcontainer.c#gimp_container_deserialize"
      ],
      "legacy_writer": [
        "app/tools/gimp-tools.c#gimp_tools_serialize",
        "app/core/gimpcontainer.c#gimp_container_serialize"
      ],
      "evidence": [
        {
          "path": "app/tools/gimp-tools.c",
          "revision": "native",
          "anchor": "gimp_config_writer_printf (writer, \"%d\", TOOL_RC_FILE_VERSION);",
          "expected": "  gimp_config_writer_printf (writer, \"%d\", TOOL_RC_FILE_VERSION);"
        },
        {
          "path": "app/tools/gimp-tools.c",
          "revision": "legacy",
          "anchor": "gimp_config_writer_printf (writer, \"%d\", TOOL_RC_FILE_VERSION);",
          "expected": "  gimp_config_writer_printf (writer, \"%d\", TOOL_RC_FILE_VERSION);"
        },
        {
          "path": "app/tools/gimp-tools.c",
          "revision": "native",
          "anchor": "#define TOOL_RC_FILE_VERSION 1",
          "expected": "#define TOOL_RC_FILE_VERSION 1"
        }
      ],
      "assertion": "config.15.toolrc.file-version.schema",
      "test_file": "migration/tests/test_persistence_config_contract.py"
    },
    {
      "id": "config.15.toolrc.item-type",
      "route": "15",
      "disposition": "saved-field",
      "storage": "GimpConfig S-expression",
      "version": "native GIMP3 source; legacy afa43fae source",
      "saved_name": "item-type",
      "native_type": "GType name identifier",
      "legacy_type": "GType name identifier",
      "owner": "GimpContainer<GimpToolItem> owned by Gimp.tool_item_list",
      "default": "Each child writes G_OBJECT_TYPE_NAME(child).",
      "absence": "Missing/unknown or non-child-type fails parse.",
      "flags": "Toolrc header and list emitted when writer exists; properties inherited from GimpToolItem/GimpToolGroup.",
      "reader": [
        "app/tools/gimp-tools.c#gimp_tools_deserialize",
        "app/core/gimpcontainer.c#gimp_container_deserialize"
      ],
      "writer": [
        "app/tools/gimp-tools.c#gimp_tools_serialize",
        "app/core/gimpcontainer.c#gimp_container_serialize"
      ],
      "legacy_reader": [
        "app/tools/gimp-tools.c#gimp_tools_deserialize",
        "app/core/gimpcontainer.c#gimp_container_deserialize"
      ],
      "legacy_writer": [
        "app/tools/gimp-tools.c#gimp_tools_serialize",
        "app/core/gimpcontainer.c#gimp_container_serialize"
      ],
      "evidence": [
        {
          "path": "app/tools/gimp-tools.c",
          "revision": "native",
          "anchor": "gimp_config_serialize (GIMP_CONFIG (container), writer, NULL)",
          "expected": "  return gimp_config_serialize (GIMP_CONFIG (container), writer, NULL);"
        },
        {
          "path": "app/tools/gimp-tools.c",
          "revision": "legacy",
          "anchor": "gimp_config_serialize (GIMP_CONFIG (container), writer, NULL)",
          "expected": "  return gimp_config_serialize (GIMP_CONFIG (container), writer, NULL);"
        },
        {
          "path": "app/tools/gimp-tools.c",
          "revision": "native",
          "anchor": "#define TOOL_RC_FILE_VERSION 1",
          "expected": "#define TOOL_RC_FILE_VERSION 1"
        }
      ],
      "assertion": "config.15.toolrc.item-type.schema",
      "test_file": "migration/tests/test_persistence_config_contract.py"
    },
    {
      "id": "config.15.toolrc.item-name",
      "route": "15",
      "disposition": "saved-field",
      "storage": "GimpConfig S-expression",
      "version": "native GIMP3 source; legacy afa43fae source",
      "saved_name": "item-name",
      "native_type": "quoted GimpObject name or NULL literal",
      "legacy_type": "quoted GimpObject name or NULL literal",
      "owner": "GimpContainer<GimpToolItem> owned by Gimp.tool_item_list",
      "default": "Serializer uses child object name; read NULL normalized empty string.",
      "absence": "Missing token fails parse.",
      "flags": "Toolrc header and list emitted when writer exists; properties inherited from GimpToolItem/GimpToolGroup.",
      "reader": [
        "app/tools/gimp-tools.c#gimp_tools_deserialize",
        "app/core/gimpcontainer.c#gimp_container_deserialize"
      ],
      "writer": [
        "app/tools/gimp-tools.c#gimp_tools_serialize",
        "app/core/gimpcontainer.c#gimp_container_serialize"
      ],
      "legacy_reader": [
        "app/tools/gimp-tools.c#gimp_tools_deserialize",
        "app/core/gimpcontainer.c#gimp_container_deserialize"
      ],
      "legacy_writer": [
        "app/tools/gimp-tools.c#gimp_tools_serialize",
        "app/core/gimpcontainer.c#gimp_container_serialize"
      ],
      "evidence": [
        {
          "path": "app/tools/gimp-tools.c",
          "revision": "native",
          "anchor": "gimp_config_serialize (GIMP_CONFIG (container), writer, NULL)",
          "expected": "  return gimp_config_serialize (GIMP_CONFIG (container), writer, NULL);"
        },
        {
          "path": "app/tools/gimp-tools.c",
          "revision": "legacy",
          "anchor": "gimp_config_serialize (GIMP_CONFIG (container), writer, NULL)",
          "expected": "  return gimp_config_serialize (GIMP_CONFIG (container), writer, NULL);"
        },
        {
          "path": "app/tools/gimp-tools.c",
          "revision": "native",
          "anchor": "#define TOOL_RC_FILE_VERSION 1",
          "expected": "#define TOOL_RC_FILE_VERSION 1"
        }
      ],
      "assertion": "config.15.toolrc.item-name.schema",
      "test_file": "migration/tests/test_persistence_config_contract.py"
    },
    {
      "id": "config.15.toolrc.item-order",
      "route": "15",
      "disposition": "dynamic-schema",
      "storage": "GimpConfig S-expression",
      "version": "native GIMP3 source; legacy afa43fae source",
      "saved_name": "item-order",
      "native_type": "ordered repeated child nodes",
      "legacy_type": "ordered repeated child nodes",
      "owner": "GimpContainer<GimpToolItem> owned by Gimp.tool_item_list",
      "default": "Serialized list order; read append TRUE.",
      "absence": "Absent entries handled by current tool registry/version fallback policy; no arbitrary numeric default.",
      "flags": "Toolrc header and list emitted when writer exists; properties inherited from GimpToolItem/GimpToolGroup.",
      "reader": [
        "app/tools/gimp-tools.c#gimp_tools_deserialize",
        "app/core/gimpcontainer.c#gimp_container_deserialize"
      ],
      "writer": [
        "app/tools/gimp-tools.c#gimp_tools_serialize",
        "app/core/gimpcontainer.c#gimp_container_serialize"
      ],
      "legacy_reader": [
        "app/tools/gimp-tools.c#gimp_tools_deserialize",
        "app/core/gimpcontainer.c#gimp_container_deserialize"
      ],
      "legacy_writer": [
        "app/tools/gimp-tools.c#gimp_tools_serialize",
        "app/core/gimpcontainer.c#gimp_container_serialize"
      ],
      "evidence": [
        {
          "path": "app/tools/gimp-tools.c",
          "revision": "native",
          "anchor": "gimp_config_serialize (GIMP_CONFIG (container), writer, NULL)",
          "expected": "  return gimp_config_serialize (GIMP_CONFIG (container), writer, NULL);"
        },
        {
          "path": "app/tools/gimp-tools.c",
          "revision": "legacy",
          "anchor": "gimp_config_serialize (GIMP_CONFIG (container), writer, NULL)",
          "expected": "  return gimp_config_serialize (GIMP_CONFIG (container), writer, NULL);"
        },
        {
          "path": "app/tools/gimp-tools.c",
          "revision": "native",
          "anchor": "#define TOOL_RC_FILE_VERSION 1",
          "expected": "#define TOOL_RC_FILE_VERSION 1"
        }
      ],
      "assertion": "config.15.toolrc.item-order.schema",
      "test_file": "migration/tests/test_persistence_config_contract.py"
    },
    {
      "id": "config.11.unknown-token",
      "route": "11",
      "disposition": "dynamic-schema",
      "storage": "GimpConfig S-expression",
      "version": "native GIMP3 source; legacy afa43fae source",
      "saved_name": "unknown-token-name -> quoted-string",
      "native_type": "UTF8 key/value strings",
      "legacy_type": "UTF8 key/value strings",
      "owner": "GimpRc unknown-token list",
      "default": "No unknown tokens at construction.",
      "absence": "Missing key absent; existing unknown key updated by set_unknown_token.",
      "flags": "Known properties handled by GParamSpec; unknown identifiers preserve quoted-string values.",
      "reader": [
        "app/config/gimprc-deserialize.c#gimp_rc_deserialize"
      ],
      "writer": [
        "app/config/gimprc-serialize.c#serialize_unknown_token"
      ],
      "legacy_reader": [
        "app/config/gimprc-deserialize.c#gimp_rc_deserialize"
      ],
      "legacy_writer": [
        "app/config/gimprc-serialize.c#serialize_unknown_token"
      ],
      "evidence": [
        {
          "path": "app/config/gimprc-serialize.c",
          "revision": "native",
          "anchor": "gimp_config_writer_string (writer, value);",
          "expected": "  gimp_config_writer_string (writer, value);"
        },
        {
          "path": "app/config/gimprc-serialize.c",
          "revision": "legacy",
          "anchor": "gimp_config_writer_string (writer, value);",
          "expected": "  gimp_config_writer_string (writer, value);"
        }
      ],
      "assertion": "config.11.unknown-token.schema",
      "test_file": "migration/tests/test_persistence_config_contract.py"
    }
  ],
  "programs": [
    {
      "path": "app/core/gimplayerpreset-apply.cpp",
      "revision": "native",
      "function": "string",
      "sha256": "dbea809cf61bce555a6656fe93ab116a5f1f7eb9c9838d0d942bc3d1c2e226d3"
    },
    {
      "path": "app/core/gimplayerpreset-apply.cpp",
      "revision": "native",
      "function": "number",
      "sha256": "4e99d3107f744b96d03fad35345ef7c9e98ac159c82afdf81cb7780c6de294cc"
    },
    {
      "path": "app/core/gimplayerpreset-apply.cpp",
      "revision": "native",
      "function": "integer",
      "sha256": "a3bb3a24abb5b7ec9c6e47c6ecb490a7e94b069c864f3a6f1ce5347c0e79c708"
    },
    {
      "path": "app/core/gimplayerpreset-apply.cpp",
      "revision": "native",
      "function": "matches",
      "sha256": "0a3d99d7d4dd09ad20d5da053f31ffd66d3b2f88827b5dab68a9ab6ebd7469ca"
    },
    {
      "path": "app/core/gimplayerpreset-apply.cpp",
      "revision": "native",
      "function": "mode",
      "sha256": "d8fefb9fc45a63f824e859a5a6bd28eed7ddc0bc1397be457dec9e9fbc6cf8da"
    },
    {
      "path": "app/core/gimplayerpreset-apply.cpp",
      "revision": "native",
      "function": "bounds",
      "sha256": "e797e1319c96cd81403dc5d13cde6b55aacf2dc959f103bc766b435e93c75f33"
    },
    {
      "path": "app/core/gimplayerpreset-apply.cpp",
      "revision": "native",
      "function": "install_filter",
      "sha256": "e07140b24a35149450dfb56fde2378c369b1c963d0422350c7a77859632d3230"
    },
    {
      "path": "app/core/gimplayerpreset-apply.cpp",
      "revision": "native",
      "function": "layers",
      "sha256": "94804ea2dec4720d5d93572ca8b2d5109201346a6584d2520a32559b4dc96a34"
    },
    {
      "path": "app/core/gimplayerpreset-apply.cpp",
      "revision": "native",
      "function": "gimp_layer_preset_apply_full",
      "sha256": "65409b39e4860e6f1427525f32c9486a218538d6aa9f3bea4895fc1cb62e3555"
    },
    {
      "path": "app/core/gimplayerpreset.c",
      "revision": "native",
      "function": "save",
      "sha256": "cc8c0ab878921b010a9f97131e1b2506c8fa27486f3dd398af4eb0398cd28cd3"
    },
    {
      "path": "app/core/gimplayerpreset.c",
      "revision": "native",
      "function": "gimp_layer_preset_new",
      "sha256": "2e97aa1c064e4afb0ab9398fd5a2c01e4e38606f9c068ac17eacf485005b11ac"
    },
    {
      "path": "app/core/gimplayerpreset.c",
      "revision": "native",
      "function": "gimp_layer_preset_set_json",
      "sha256": "4fdc735458031d9caf0c5f8dccbb14f4a3385cc09fdc268d3f6275320f2cedf4"
    },
    {
      "path": "app/core/gimplayerpreset.c",
      "revision": "native",
      "function": "gimp_layer_preset_load",
      "sha256": "6f61efe4e19bab289b1577002529106a77bd9c3c7d8a2666f9463fcb30e57008"
    },
    {
      "path": "app/presets/layer-preset.cpp",
      "revision": "legacy",
      "function": "is_source_valid",
      "sha256": "06161b811606031ba9a9b54f1ade0581c126242cbc620f2322630ef84c125e26"
    },
    {
      "path": "app/presets/layer-preset.cpp",
      "revision": "legacy",
      "function": "get_boundary",
      "sha256": "4c345fe34aabbbe7314779372428bdb24d0899d08be8874623c61c19c5d64cde"
    },
    {
      "path": "app/presets/layer-preset.cpp",
      "revision": "legacy",
      "function": "get_source",
      "sha256": "c5d267240e38b7b376700e421ded201d24c02d1cc47970cb679a5a77b4e8d459"
    },
    {
      "path": "app/presets/layer-preset.cpp",
      "revision": "legacy",
      "function": "get_mode",
      "sha256": "c560cf12c00c3dbd11572003197e12c26646aa51a5887dc790f01f88740cd7cc"
    },
    {
      "path": "app/presets/layer-preset.cpp",
      "revision": "legacy",
      "function": "get_opacity",
      "sha256": "4f7e117d058f4215ea2c54fa6f629f6325a1371a787c8055c4eb1847b48e41b2"
    },
    {
      "path": "app/presets/layer-preset.cpp",
      "revision": "legacy",
      "function": "get_alpha",
      "sha256": "24f487b0cb40f55ef9a32c1b2319cf044c852077455263f16fe3c96e587fbe29"
    },
    {
      "path": "app/presets/layer-preset.cpp",
      "revision": "legacy",
      "function": "get_args",
      "sha256": "6ffec18087950b935cdd79073cdcd6bc07967ddfb04701f4d3f6dcf43f0e1b96"
    },
    {
      "path": "app/presets/layer-preset.cpp",
      "revision": "legacy",
      "function": "create_one_layer",
      "sha256": "8c86f6d7a3f3bd70de7f16f1b43f107899cfea927e12b3bc3f4a8d1643a48864"
    },
    {
      "path": "app/presets/layer-preset.cpp",
      "revision": "legacy",
      "function": "apply_for",
      "sha256": "a7854f8aeb84ea1ae0fdb74bcaef6ea7585c46885fba276946279f018cd7aac5"
    },
    {
      "path": "app/presets/gimpjsonresource.cpp",
      "revision": "legacy",
      "function": "JsonResource::load",
      "sha256": "8ea4df0b358d20c8df93cbec50525aa754dd5b2a80871b7e0a2006727a3c899b"
    },
    {
      "path": "app/presets/gimpjsonresource.cpp",
      "revision": "legacy",
      "function": "JsonResource::save",
      "sha256": "78c4c821e4a07d638d206b681783faa20c2b80d3a2f7fa786b810aef9bc864bc"
    },
    {
      "path": "app/core/gimpparasitelist.c",
      "revision": "native",
      "function": "parasite_serialize",
      "sha256": "8084433fc1228582159ba81f849fb92ed64d4650abc8e886f175d96cf91544a3"
    },
    {
      "path": "app/core/gimpparasitelist.c",
      "revision": "native",
      "function": "gimp_parasite_list_deserialize",
      "sha256": "8aeba7f6c3205b081b847a786e4d02b056fb6fa4eafc0ebdfcf0b1500e79af57"
    },
    {
      "path": "app/core/gimpparasitelist.c",
      "revision": "legacy",
      "function": "parasite_serialize",
      "sha256": "71a49e1d072afddd8da7f0d7a4815f546c94e29ed6cb7205a328ee08fe959f67"
    },
    {
      "path": "app/core/gimpparasitelist.c",
      "revision": "legacy",
      "function": "gimp_parasite_list_deserialize",
      "sha256": "8aeba7f6c3205b081b847a786e4d02b056fb6fa4eafc0ebdfcf0b1500e79af57"
    },
    {
      "path": "app/tools/gimp-tools.c",
      "revision": "native",
      "function": "gimp_tools_serialize",
      "sha256": "68a5fe66f6f66b53a69b422ff7f48993fba1d801a90c0b5b6444d8058a746405"
    },
    {
      "path": "app/tools/gimp-tools.c",
      "revision": "native",
      "function": "gimp_tools_deserialize",
      "sha256": "e405fedb7c411bd6eb7ee119f11db0935a6335f1940b384842eef8bf471fd05f"
    },
    {
      "path": "app/tools/gimp-tools.c",
      "revision": "native",
      "function": "gimp_tools_clear_structure",
      "sha256": "58f8e5f1f0eaed15cc5248791e9371f34d1fb2721211c5abf566497cb5ca6cd1"
    },
    {
      "path": "app/tools/gimp-tools.c",
      "revision": "legacy",
      "function": "gimp_tools_serialize",
      "sha256": "68a5fe66f6f66b53a69b422ff7f48993fba1d801a90c0b5b6444d8058a746405"
    },
    {
      "path": "app/tools/gimp-tools.c",
      "revision": "legacy",
      "function": "gimp_tools_deserialize",
      "sha256": "4f59ffd422a11d9467256953b5dba22d64e9dcc5190bd495781603af3cf5b688"
    },
    {
      "path": "app/core/gimpcontainer.c",
      "revision": "native",
      "function": "gimp_container_serialize_foreach",
      "sha256": "35e3e14fcfd6e54e5798e94f4016e623fae930401c26702468a2a9838a6d5208"
    },
    {
      "path": "app/core/gimpcontainer.c",
      "revision": "native",
      "function": "gimp_container_deserialize",
      "sha256": "43bdaeea7e47462b398027576b743012e84df746e3124be4353bf1f6bf007810"
    },
    {
      "path": "app/core/gimpcontainer.c",
      "revision": "legacy",
      "function": "gimp_container_serialize_foreach",
      "sha256": "5aea2a1b87b7442ab5937f93a3da256d3e45342ff8f38bae10a43b64f05b076a"
    },
    {
      "path": "app/core/gimpcontainer.c",
      "revision": "legacy",
      "function": "gimp_container_deserialize",
      "sha256": "9cefcdd95b06cf08b42232ed780a8b4ba9c72dc50c5bb1432584fbb5496f88bc"
    },
    {
      "path": "app/config/gimprc-serialize.c",
      "revision": "native",
      "function": "serialize_unknown_token",
      "sha256": "9312dbc98bd01cd0982cf47525de1d639fed0538c6b9cef97bf66bf0970fe271"
    },
    {
      "path": "app/config/gimprc-serialize.c",
      "revision": "legacy",
      "function": "serialize_unknown_token",
      "sha256": "9312dbc98bd01cd0982cf47525de1d639fed0538c6b9cef97bf66bf0970fe271"
    },
    {
      "path": "app/core/gimpjsonresource.c",
      "revision": "native",
      "function": "gimp_json_resource_set_json",
      "sha256": "dc265a908b5c4af20e9a0d6460d8cc14da4c7e17ef404926d87db517bfc6b3c9"
    },
    {
      "path": "app/core/gimpjsonresource.c",
      "revision": "native",
      "function": "gimp_json_resource_ref_json",
      "sha256": "642938fba93fabd852927b06237df4dd99122a9a0d11416989a803efc0495666"
    },
    {
      "path": "app/core/gimpjsonresource.c",
      "revision": "native",
      "function": "gimp_json_resource_is_live",
      "sha256": "54b1839a2573a0e2f8bef9895d883d24f3de4120dbc2f87800413b34e203967e"
    },
    {
      "path": "app/core/gimpjsonresource.c",
      "revision": "native",
      "function": "save",
      "sha256": "a17de8e2c709fb41d2dbbd95f133d484321f325a3d9a4ee9dea5b136c919f7b3"
    },
    {
      "path": "app/core/gimpjsonresource.c",
      "revision": "native",
      "function": "copy",
      "sha256": "e49218c66313d773e0a2c13e953cfb32e57ad163eddd8a0de9fd3d6c0239b2e3"
    },
    {
      "path": "app/core/gimpjsonresource.c",
      "revision": "native",
      "function": "prepare_root",
      "sha256": "ff8a4e572d8f8d23048e065134c9ab5a40922dd465e41f5d7aa69fb9c471bb2b"
    },
    {
      "path": "app/core/gimpjsonresource.c",
      "revision": "native",
      "function": "dispose",
      "sha256": "e2c1d6848c5367bc70b6fffb2e0f9088840c9fc5e002aa9119146db5ba574180"
    },
    {
      "path": "app/core/gimpjsonresource.c",
      "revision": "native",
      "function": "gimp_json_resource_class_init",
      "sha256": "d226bedb353cb9b77e138de27d34e351d5c4e6efbc442c1e463d00fc8476499e"
    },
    {
      "path": "app/core/gimplayerpreset.c",
      "revision": "native",
      "function": "prepare_root",
      "sha256": "96069361080acedfd47093e3854d2d3cb14314e7f1217905fc12071c6b587e8b"
    },
    {
      "path": "app/core/gimplayerpreset.c",
      "revision": "native",
      "function": "root_changed",
      "sha256": "e22c0d7cb136174a63dd7415deadc3146423c47d83fb1b642aca1f50363bb357"
    },
    {
      "path": "app/core/gimplayerpreset.c",
      "revision": "native",
      "function": "gimp_layer_preset_dup_json",
      "sha256": "63f7ba7e983a8d08327c602cfc14c439cb9e009779b37ac3dedebda2b2050baf"
    },
    {
      "path": "app/core/gimplayerpreset-apply.cpp",
      "revision": "native",
      "function": "gimp_layer_preset_matches",
      "sha256": "32b7285528e3cf56fc339bd550bfdb9e263a61e6bf2780a4bddd002ea2f6c236"
    }
  ]
}''')

if __name__=='__main__':main()
