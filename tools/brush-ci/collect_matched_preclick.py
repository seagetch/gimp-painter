#!/usr/bin/env python3
"""Bounded public metrics from private Meson JSONL; never an acceptance gate."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import statistics

CELLS = ('baseline-normal','current-normal','baseline-sanitizer','current-sanitizer')
ORDER = [('baseline-normal',1),('current-normal',1),('baseline-sanitizer',1),('current-sanitizer',1),
         ('current-sanitizer',2),('baseline-sanitizer',2),('current-normal',2),('baseline-normal',2),
         ('baseline-normal',3),('current-normal',3),('baseline-sanitizer',3),('current-sanitizer',3)]
PRIMARY='/matched-preclick/primary'
FIDELITY='/brush-thumbnail-native-popup/private-target-release'
LABELS={'toplevel-draw','target-view-draw','frame-update','frame-layout','frame-paint'}
FIELDS=set('acceptance_eligible extended primary_pass primary_elapsed_us primary_cpu_us primary_frames primary_stable primary_iterations longest_iteration_us first_frame_us stable_at_us final_elapsed_us final_frames final_stable geometry_changes initial_mapped initial_realized initial_viewable setup_us factory_us show_drain_us'.split())
MAX_INPUT=32*1024*1024
MAX_OUTPUT=1024*1024
FINDINGS={'no-finding-reported','leak-only-reported-order-unknown','leak-only-report-emitted-after-test-completion','fault-reported-order-unknown','incomplete-sanitizer-report','unknown-diagnostic'}
LEAK_HEADER=re.compile(r'^(?:==(?:matched-preclick==|painter-compact-options==)?[0-9]{1,20}==)?ERROR: LeakSanitizer: detected memory leaks$',re.M)
class Invalid(Exception): pass

def require(value):
    if not value: raise Invalid()

def number(value):
    require(isinstance(value,str) and re.fullmatch(r'[0-9]{1,18}',value))
    return int(value)

def pairs(line):
    out={}
    for item in line.split():
        parts=item.split('=');require(len(parts)==2 and parts[0] not in out)
        out[parts[0]]=number(parts[1])
    return out

def unique(items):
    out={}
    for k,v in items:
        require(k not in out);out[k]=v
    return out

def read_jsonl(path):
    p=Path(path);require(p.is_file() and p.stat().st_size<=MAX_INPUT)
    with p.open('rb') as stream:raw=stream.read(MAX_INPUT+1)
    require(len(raw)<=MAX_INPUT)
    lines=raw.decode('utf-8').splitlines(); require(len(lines)==1)
    record=json.loads(lines[0],object_pairs_hook=unique);require(isinstance(record,dict))
    if 'stderr' not in record:record['stderr']=''
    require(isinstance(record.get('stdout'),str) and isinstance(record.get('stderr'),str))
    require(type(record.get('returncode')) is int)
    require(record.get('result') in ('OK','FAIL','ERROR','TIMEOUT','INTERRUPT','SKIP','EXPECTEDFAIL','UNEXPECTEDPASS'))
    return record,hashlib.sha256(raw).hexdigest()

def clean_lines(text):
    return [x[2:] if x.startswith('# ') else x for x in text.splitlines()]

def phase_records(lines, only_first=False):
    summaries=[];events=[]
    summary=re.compile(r'Geometry timing wait=1 probe=([a-z-]+) hooked=([01]) entries=(\d+) exits=(\d+) unmatched=(\d+) orphan-exits=(\d+) total-us=(\d+) longest-us=(\d+)')
    event=re.compile(r'Geometry timing wait=1 probe=([a-z-]+) event=(enter|exit) elapsed-us=(\d+)')
    for line in lines:
        if not line.startswith('Geometry timing wait=1 '):
            require(only_first or not line.startswith('Geometry timing '))
            continue
        m=summary.fullmatch(line)
        if m:
            require(m[1] in LABELS)
            summaries.append(dict(zip(('probe','hooked','entries','exits','unmatched','orphan_exits','total_us','longest_us'),[m[1]]+[number(v) for v in m.groups()[1:]])))
        else:
            m=event.fullmatch(line);require(m and m[1] in LABELS)
            events.append({'probe':m[1],'event':m[2],'elapsed_us':number(m[3])})
        require(len(summaries)<=5 and len(events)<=32)
    require(len({x['probe'] for x in summaries})==len(summaries))
    return summaries,events

def primary_metrics(lines):
    summary=[x for x in lines if x.startswith('MATCHED_PRECLICK ')]
    require(len(summary)==1)
    records=[x for x in lines if x.startswith('MATCHED_')]
    require(records and records[0]==summary[0] and records[-1].startswith('MATCHED_FRAMES '))
    require(all(x.startswith('MATCHED_FRAME ') for x in records[1:-1]))
    require(all(i<lines.index(summary[0]) for i,x in enumerate(lines) if x.startswith('Geometry timing ')))
    m=pairs(summary[0].split(' ',1)[1]);require(set(m)==FIELDS)
    require(m['acceptance_eligible']==0 and m['extended']==0)
    require(m['primary_pass'] in (0,1) and bool(m['primary_pass'])==(m['primary_stable']>=3))
    require(m['primary_frames']>0 and m['primary_iterations']>0)
    require(m['primary_frames']==m['final_frames'] and m['primary_stable']==m['final_stable'])
    require(m['longest_iteration_us']<=m['primary_elapsed_us']<=m['final_elapsed_us'])
    require(m['first_frame_us']<=m['primary_elapsed_us'])
    require(m['stable_at_us']==0 or m['stable_at_us']<=m['primary_elapsed_us'])
    require(all(m[k] in (0,1) for k in ('initial_mapped','initial_realized','initial_viewable')))
    frames=[];ending=[]
    for line in lines:
        if line.startswith('MATCHED_FRAME '):
            f=pairs(line.split(' ',1)[1]);require(set(f)=={'ordinal','elapsed_us','stable'})
            require(len(frames)<64 and f['ordinal']==len(frames)+1 and f['elapsed_us']<=m['final_elapsed_us'])
            require(not frames or f['elapsed_us']>=frames[-1]['elapsed_us'])
            frames.append(f)
        elif line.startswith('MATCHED_FRAMES '): ending.append(pairs(line.split(' ',1)[1]))
        elif line.startswith('MATCHED_') and not line.startswith('MATCHED_PRECLICK '): raise Invalid()
    require(len(frames)==min(m['final_frames'],64) and len(ending)==1)
    require(ending[0]=={'truncated':int(m['final_frames']>64)})
    require(frames[0]['elapsed_us']==m['first_frame_us'] and frames[0]['stable']==0)
    require(all(f['stable'] in (0,frames[i-1]['stable']+1) for i,f in enumerate(frames) if i))
    require(m['primary_stable']<m['primary_frames'])
    if not ending[0]['truncated']:
        require(frames[-1]['stable']==m['final_stable'])
        require(sum(f['stable']==0 for f in frames[1:])==m['geometry_changes'])
        stable_times=[f['elapsed_us'] for f in frames if f['stable']>=3]
        require(m['stable_at_us']==(stable_times[0] if stable_times else 0))
    phases,events=phase_records(lines)
    require(len(phases)==5)
    return {'summary':m,'frames':frames,'phases':phases,'phase_events':events,
            'frame_output_truncated':bool(ending[0]['truncated']),
            'original_primary_pass':bool(m['primary_pass']),
            'elapsed_over_3s':m['primary_elapsed_us']>3000000,
            'elapsed_excess_over_3s_us':max(0,m['primary_elapsed_us']-3000000),
            'cpu_to_wall_ratio':round(m['primary_cpu_us']/m['primary_elapsed_us'],6) if m['primary_elapsed_us'] else None}

def fidelity_metrics(lines):
    pattern=re.compile(r'Geometry wait=1 caller=brush_popup_open:425 elapsed-us=(\d+) iterations=(\d+) callbacks=(\d+) stable=(\d+) changes=(\d+) longest-dispatch-us=(\d+) initial-mapped/realized/viewable=([01])/([01])/([01]) final-mapped/realized/viewable=([01])/([01])/([01]) same-clock=([01])')
    candidates=[x for x in lines if x.startswith('Geometry wait=1 caller=')]
    require(len(candidates)==1)
    match=pattern.fullmatch(candidates[0]);require(match)
    names=('elapsed_us','iterations','frames','stable','changes','longest_iteration_us','initial_mapped','initial_realized','initial_viewable','final_mapped','final_realized','final_viewable','same_clock')
    m=dict(zip(names,[number(v) for v in match.groups()]));require(m['frames']>0 and m['iterations']>0 and m['longest_iteration_us']<=m['elapsed_us'])
    phases,events=phase_records(lines,only_first=True)
    return {'summary':m,'original_primary_pass':m['stable']>=3,'elapsed_over_3s':m['elapsed_us']>3000000,
            'elapsed_excess_over_3s_us':max(0,m['elapsed_us']-3000000),'phases':phases,'phase_events':events}

def sanitizer(stdout,stderr):
    combined=stdout+'\n'+stderr
    leak=bool(LEAK_HEADER.search(combined))
    other=bool(re.search(r'ERROR: AddressSanitizer:|AddressSanitizer:DEADLYSIGNAL|UndefinedBehaviorSanitizer|runtime error:|ERROR: ThreadSanitizer|Sanitizer CHECK failed',combined))
    if other:return 'fault-reported-order-unknown'
    remainder=LEAK_HEADER.sub('',combined)
    if re.search(r'(?im)\b(?:CRITICAL|WARNING|ERROR|FATAL)\b|Bail out!|assertion.*failed',remainder):return 'unknown-diagnostic'
    if leak:
        if re.search(r'SUMMARY: AddressSanitizer: \d+ byte\(s\) leaked in \d+ allocation\(s\)',combined):return 'leak-only-reported-order-unknown'
        return 'incomplete-sanitizer-report'
    if re.search(r'(?i)sanitizer|\bSIG(?:SEGV|ABRT|BUS)\b',combined):return 'unknown-diagnostic'
    return 'no-finding-reported'

def ordered_emission(r,leaf):
    command=r.get('command')
    if not isinstance(command,list) or len(command)!=5 or not all(isinstance(x,str) for x in command):return False
    wrapper=Path(__file__).with_name('matched_preclick_exec.py').resolve()
    try:
        if Path(command[1]).resolve(strict=True)!=wrapper:return False
        if Path(command[0]).name not in ('python3','python3.13'):return False
        expected='matched-preclick' if leaf==PRIMARY else 'painter-compact-options'
        if Path(command[2]).name!=expected or command[3:]!=['-p',leaf]:return False
        if r['stderr']!='':return False
        return True
    except OSError:return False

def late_leak_report(r,leaf):
    if not ordered_emission(r,leaf):return False
    text=r['stdout']
    tap=list(re.finditer(r'^ok 1 '+re.escape(leaf)+r'\s*$',text,re.M))
    plans=list(re.finditer(r'^1\.\.1\s*$',text,re.M))
    leaks=list(LEAK_HEADER.finditer(text))
    summaries=list(re.finditer(r'SUMMARY: AddressSanitizer: \d+ byte\(s\) leaked in \d+ allocation\(s\)',text))
    if len(tap)!=1 or len(plans)!=1 or len(leaks)!=1 or len(summaries)!=1:return False
    if not(tap[0].end()<=plans[0].start()<leaks[0].start()<summaries[0].start()):return False
    if leaf==PRIMARY:
        end=list(re.finditer(r'^# MATCHED_FRAMES truncated=0\s*$',text,re.M))
        if len(end)!=1 or end[0].end()>tap[0].start():return False
    return True

def collect(log,cell,repetition,order,kind):
    report={'format':1,'acceptance_eligible':False,'kind':kind if kind in ('primary','fidelity') else 'invalid','cell':cell if cell in CELLS else 'invalid','repetition':repetition,'order':order,
            'collection_complete':False,'metrics_available':False,'comparison_eligible':False,'status':'inconclusive'}
    try:
        require(cell in CELLS and kind in ('primary','fidelity'))
        if kind=='primary': require(1<=order<=12 and ORDER[order-1]==(cell,repetition))
        else:require(repetition==1 and (order,cell) in ((13,'current-normal'),(14,'current-sanitizer')))
        r,digest=read_jsonl(log)
        report['raw_log_sha256']=digest
        report['collector_sha256']=hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
        name='matched-preclick' if kind=='primary' else 'painter-compact-options'
        require(re.fullmatch(r'gimp:(?:app|diagnostic-only) / '+name,r.get('name','')))
        result=r['result'];code=r['returncode'];require(-255<=code<=255)
        report['process']={'meson_result':result,'returncode':code,'passed':result=='OK' and code==0}
        finding=sanitizer(r['stdout'],r['stderr']);report['sanitizer_status']=finding
        report['diagnostic_ordering']='not-established-from-split-streams' if finding!='no-finding-reported' else 'no-finding-reported'
        leaf=PRIMARY if kind=='primary' else FIDELITY
        taps=re.findall(r'^(ok|not ok) (\d+) (/[A-Za-z0-9_/-]+)(.*)$',r['stdout'],re.M)
        plans=re.findall(r'^1\.\.(\d+)\s*$',r['stdout'],re.M)
        tap_ok=len(taps)==1 and taps[0][:3]==('ok','1',leaf) and not taps[0][3].strip() and plans==['1']
        require(not taps or (len(taps)==1 and taps[0][2]==leaf))
        report['tap_status']='exact-leaf-pass' if tap_ok else 'incomplete-or-failed'
        ordered=ordered_emission(r,leaf)
        report['ordered_emission_verified']=ordered
        if ordered:report['exec_wrapper_sha256']=hashlib.sha256(Path(__file__).with_name('matched_preclick_exec.py').read_bytes()).hexdigest()
        late_leak=finding=='leak-only-reported-order-unknown' and tap_ok and late_leak_report(r,leaf)
        if late_leak:
            finding='leak-only-report-emitted-after-test-completion'
            report['sanitizer_status']=finding
            report['diagnostic_ordering']='report-emission-after-complete-tap-not-allocation-time'
            report['leak_qualification']='Native process remains failed; emitted report ordering does not establish harmlessness or allocation timing.'
        require(result not in ('TIMEOUT','INTERRUPT','SKIP') and code!=77)
        require(not re.search(r'(?im)^.*(?:#\s*SKIP|#\s*TODO).*$',r['stdout']))
        lines=clean_lines(r['stdout'])
        report['metrics']=primary_metrics(lines) if kind=='primary' else fidelity_metrics(lines)
        report['metrics_available']=True
        report['collection_complete']=tap_ok
        valid=tap_ok and ((report['process']['passed'] and finding=='no-finding-reported') or (late_leak and result=='FAIL' and code==1))
        if kind=='primary':
            valid=valid and not report['metrics']['frame_output_truncated'] and all(report['metrics']['summary'][k]==1 for k in ('initial_mapped','initial_realized','initial_viewable'))
        report['comparison_eligible']=bool(valid)
        report['status']='qualified-late-leak-metrics' if valid and late_leak else 'collected' if valid else 'qualified-metrics-inconclusive'
        return report
    except (Invalid,ValueError,TypeError,KeyError,OSError,RecursionError):
        report['failure_category']='invalid-or-incomplete-collection'
        return report

def write(path,value):
    encoded=json.dumps(value,indent=2)+'\n'
    require(len(encoded.encode())<=MAX_OUTPUT)
    Path(path).write_text(encoded)

BASE='e25a94e21efca6e6604ddac6f15570c049f421e7'
CURRENT='b671825309326bdb55571652e43ab78cc83fd92d'
TREES={BASE:'8634eeef49c81aaa5c25192cb76d749ff680683f',CURRENT:'4469df41e161e3f855918125932e377757a1c937'}

def bounded_json(path):
    p=Path(path);require(p.is_file() and p.stat().st_size<=MAX_OUTPUT)
    j=json.loads(p.read_text(),object_pairs_hook=unique);require(isinstance(j,dict));return j

def hexhash(v):
    return isinstance(v,str) and re.fullmatch('[0-9a-f]{64}',v) is not None

def integer(v):
    require(type(v) is int and 0<=v<10**18);return v

def expected_configuration_overlay(cell):
    # Fixed public pins duplicated intentionally: validating a receipt must not
    # execute the producer module whose hash was checked above.
    baseline = cell.startswith('baseline-')
    return {'policy_sha256':'97a542bfdf237f7680b6e865ca728e4263eb244b729a8b2b0751f1f4d028c765',
            'path':'meson.build', 'applied':baseline,
            'scope':'diagnostic-build-configuration-only',
            'before_sha256':('6cbbc986cae81fcbcd3234e70eac91d96ffa2a254ad8c1b0e56f64f19325959f' if baseline else '74358058b211605bcd3fcb0e08df60722728196b93102799bc27ec0500329ea5'),
            'after_sha256':('83d371852ebeeede92caee7e0280f35dfc28a2a05cc8af6d00a84b67177d5b0f' if baseline else '74358058b211605bcd3fcb0e08df60722728196b93102799bc27ec0500329ea5')}


def aggregate(reports,identities):
    identity=bounded_json(identities)
    require(identity.get('schema')=='matched-preclick-comparison-identity-v1' and identity.get('acceptance_eligible') is False)
    require(type(identity.get('comparison_eligible')) is bool and type(identity.get('closure_comparable')) is bool)
    require(set(identity['cells'])==set(CELLS) and isinstance(identity['errors'],list))
    require(hexhash(identity.get('producer_sha256')))
    producer=Path(__file__).with_name('matched_provenance.py')
    require(producer.is_file() and hashlib.sha256(producer.read_bytes()).hexdigest()==identity['producer_sha256'])
    identity_ok=identity['comparison_eligible'] and identity['closure_comparable'] and not identity['errors']
    refs={}
    for cell in CELLS:
        c=identity['cells'][cell];commit=BASE if cell.startswith('baseline-') else CURRENT
        require(c['source_commit']==commit and c['source_tree']==TREES[commit])
        require(type(c['closure_comparable']) is bool and hexhash(c['identity_sha256']))
        inventories=c['runtime_inventory_sha256'];require(isinstance(inventories,list))
        require(len(inventories)==(3 if cell.startswith('baseline-') else 4) and len(set(inventories))==len(inventories) and all(hexhash(v) for v in inventories))
        identity_ok=identity_ok and c['closure_comparable']
        configuration=c.get('configuration_overlay')
        require(configuration==expected_configuration_overlay(cell) and type(configuration.get('applied')) is bool)
        refs[cell]={'identity_sha256':c['identity_sha256'],'runtime_inventory_sha256':inventories,
                    'configuration_overlay':expected_configuration_overlay(cell)}
    bindings=identity.get('runs');require(isinstance(bindings,list) and len(bindings)==14)
    bound={};used_inventories=set()
    for expected,b in enumerate(bindings,1):
        require(isinstance(b,dict) and type(b.get('order')) is int and b['order']==expected)
        cell,rep=(ORDER+[('current-normal',1),('current-sanitizer',1)])[expected-1]
        require(b.get('cell')==cell and type(b.get('repetition')) is int and b['repetition']==rep)
        require(b.get('kind')==('primary' if expected<=12 else 'fidelity'))
        require(hexhash(b.get('raw_log_sha256')) and hexhash(b.get('runtime_inventory_sha256')))
        inventory=b['runtime_inventory_sha256']
        require(inventory in refs[cell]['runtime_inventory_sha256'] and inventory not in used_inventories)
        used_inventories.add(inventory);bound[expected]=b
    collector_hash=hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
    paths=sorted(Path(reports).glob('*.json'));require(len(paths)==14)
    rows=[];seen=set();eligible=bool(identity_ok)
    for path in paths:
        r=bounded_json(path);require(r.get('format')==1 and r.get('acceptance_eligible') is False)
        cell=r['cell'];kind=r['kind'];rep=integer(r['repetition']);order=integer(r['order'])
        require(order not in seen and order in bound);seen.add(order)
        b=bound[order]
        require((cell,rep,kind)==(b['cell'],b['repetition'],b['kind']))
        require(r.get('raw_log_sha256')==b['raw_log_sha256'] and r.get('collector_sha256')==collector_hash)
        if kind=='primary':require(1<=order<=12 and ORDER[order-1]==(cell,rep))
        else:require(kind=='fidelity' and rep==1 and (order,cell) in ((13,'current-normal'),(14,'current-sanitizer')))
        require(type(r['comparison_eligible']) is bool and type(r['metrics_available']) is bool)
        process=r.get('process');require(isinstance(process,dict))
        require(type(process.get('returncode')) is int and -255<=process['returncode']<=255 and type(process.get('passed')) is bool)
        require(r.get('sanitizer_status') in FINDINGS and r.get('tap_status') in ('exact-leaf-pass','incomplete-or-failed'))
        require(process['passed']==(process.get('meson_result')=='OK' and process['returncode']==0))
        require(type(r.get('collection_complete')) is bool)
        if r['comparison_eligible']:
            require(r['metrics_available'] and r['collection_complete'] and r.get('tap_status')=='exact-leaf-pass')
            finding=r.get('sanitizer_status')
            if finding=='no-finding-reported':require(process['passed'])
            else:
                require(finding=='leak-only-report-emitted-after-test-completion' and process['meson_result']=='FAIL' and process['returncode']==1)
                require(r.get('ordered_emission_verified') is True and r.get('exec_wrapper_sha256')==hashlib.sha256(Path(__file__).with_name('matched_preclick_exec.py').read_bytes()).hexdigest())
        eligible=eligible and r['comparison_eligible']
        row={'raw_log_sha256':b['raw_log_sha256'],'runtime_inventory_sha256':b['runtime_inventory_sha256'],'cell':cell,'kind':kind,'repetition':rep,'order':order,'comparison_eligible':r['comparison_eligible'],'metrics_available':r['metrics_available'],'native_process_passed':process['passed'],'native_returncode':process['returncode'],'tap_status':r['tap_status'],'sanitizer_status':r['sanitizer_status'],'collection_complete':r['collection_complete']}
        if r['metrics_available']:
            m=r['metrics'];require(type(m['original_primary_pass']) is bool)
            summary=m['summary'];elapsed=integer(summary['primary_elapsed_us'] if kind=='primary' else summary['elapsed_us'])
            longest=integer(summary['longest_iteration_us']);require(longest<=elapsed)
            row.update(original_primary_pass=m['original_primary_pass'],elapsed_us=elapsed,longest_iteration_us=longest)
            if kind=='primary':row['thread_cpu_us']=integer(summary['primary_cpu_us'])
        rows.append(row)
    require(seen==set(range(1,15)))
    stats={}
    for cell in CELLS:
        selected=[r for r in rows if r['cell']==cell and r['kind']=='primary' and r['metrics_available']]
        vals=[r['elapsed_us'] for r in selected]
        stats[cell]={'metrics_population':'all-available-including-inconclusive','comparison_eligible_repetitions':sum(r['comparison_eligible'] for r in selected),'available_repetitions':len(vals),'primary_passes':sum(r['original_primary_pass'] for r in selected),
                     'median_elapsed_us':statistics.median(vals) if vals else None,'min_elapsed_us':min(vals) if vals else None,'max_elapsed_us':max(vals) if vals else None}
    return {'format':1,'acceptance_eligible':False,'comparison_eligible':bool(eligible),
            'status':'descriptive-only' if eligible else 'inconclusive',
            'identity_comparable':bool(identity_ok),'identity_refs':refs,
            'baseline_build_metadata_modified':True,'pristine_baseline_build_comparison':False,
            'samples':sorted(rows,key=lambda r:r['order']),'statistics':stats,
            'interpretation':'Baseline root Meson metadata has an exact diagnostic compatibility overlay in both modes; this is not a pristine-baseline build comparison. Three repetitions are descriptive, not significance or causality proof. Fidelity needs human review; no acceptance promotion.'}

def main():
    p=argparse.ArgumentParser();sub=p.add_subparsers(dest='action',required=True)
    c=sub.add_parser('collect');c.add_argument('--log',required=True);c.add_argument('--cell',choices=CELLS,required=True)
    c.add_argument('--repetition',type=int,required=True);c.add_argument('--order',type=int,required=True);c.add_argument('--kind',choices=('primary','fidelity'),required=True);c.add_argument('--output',required=True)
    a=sub.add_parser('aggregate');a.add_argument('--reports',required=True);a.add_argument('--identities',required=True);a.add_argument('--output',required=True)
    args=p.parse_args()
    if args.action=='collect':report=collect(args.log,args.cell,args.repetition,args.order,args.kind)
    else:
        try:report=aggregate(args.reports,args.identities)
        except (Invalid,ValueError,TypeError,KeyError,OSError,RecursionError):report={'format':1,'acceptance_eligible':False,'status':'inconclusive','failure_category':'invalid-aggregate-input','comparison_eligible':False}
    write(args.output,report)
    return 0 if report.get('comparison_eligible') else 2
if __name__=='__main__':raise SystemExit(main())
