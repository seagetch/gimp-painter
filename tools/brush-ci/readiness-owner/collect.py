#!/usr/bin/env python3
"""Separate after-failure controller: bounded signal timing, never acceptance.
Reuses unchanged owned-child controller; no inferior expressions or attachment.
"""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import sys

BASE_PATH = Path(__file__).resolve().parents[1]/'sample_case14.py'
spec = importlib.util.spec_from_file_location('owner_base', BASE_PATH)
base = importlib.util.module_from_spec(spec); spec.loader.exec_module(base)
phase_spec=importlib.util.spec_from_file_location('owner_phase_records',Path(__file__).with_name('phase_records.py'))
phase_module=importlib.util.module_from_spec(phase_spec);phase_spec.loader.exec_module(phase_module)
leaf_spec=importlib.util.spec_from_file_location('owner_leaf_records',Path(__file__).with_name('leaf_records.py'))
leaf_module=importlib.util.module_from_spec(leaf_spec);leaf_spec.loader.exec_module(leaf_module)
image_spec=importlib.util.spec_from_file_location('owner_image_records',Path(__file__).with_name('image_records.py'))
image_module=importlib.util.module_from_spec(image_spec);image_spec.loader.exec_module(image_module)
icon_spec=importlib.util.spec_from_file_location('owner_icon_records',Path(__file__).with_name('icon_records.py'))
icon_module=importlib.util.module_from_spec(icon_spec);icon_spec.loader.exec_module(icon_module)
FIELDS = {'summary':4, 'window':5, 'probe':11, 'event':3, 'complete':0}
MAX_RECORDS = 1 + 16 + 32 + 32 + 1

MAX_TARGET_LINE = 4096

class Records:
    def __init__(self):
        self.rows=[]; self.complete=False; self.invalid=False
        self.pending=''; self.transport=None
    def consume(self, line):
        # Quoted MI target streams are chunks, not logical lines. Keep a bounded
        # target-only remainder; never join debugger console/log or raw output.
        if not isinstance(line,str): return
        if line.startswith('@'):
            try: chunk=json.loads(line[1:])
            except (ValueError, TypeError): self.invalid=True; return
            if not isinstance(chunk,str): self.invalid=True; return
            for fragment in chunk.splitlines(keepends=True):
                self.pending += fragment
                if len(self.pending)>MAX_TARGET_LINE:
                    self.invalid=True; self.pending=''; return
                if fragment.endswith('\n'):
                    self.consume_row(self.pending[:-1].removesuffix('\r'), 'target')
                    self.pending=''
            return
        if line.startswith('OWNER_DIAG '):
            if self.pending: self.invalid=True; return
            self.consume_row(line, 'raw')
    def consume_row(self, text, transport):
        if not text.startswith('OWNER_DIAG '): return
        if self.transport is not None and self.transport != transport:
            self.invalid=True; return
        self.transport=transport
        tokens=text.split(' '); kind=tokens[1] if len(tokens)>1 else ''
        if (self.complete or len(self.rows)>=MAX_RECORDS or kind not in FIELDS or
            len(tokens)!=2+FIELDS[kind] or
            any(not re.fullmatch(r'[0-9]{1,15}', x) for x in tokens[2:])):
            self.invalid=True; return
        values=[int(x) for x in tokens[2:]]
        self.rows.append({'kind':kind,'values':values})
        if kind=='complete': self.complete=True
    def result(self):
        # Require full shape and indexes, not merely an arbitrary finish marker.
        try:
            assert self.complete and not self.invalid and not self.pending
            assert self.rows[0]['kind']=='summary' and self.rows[-1]['kind']=='complete'
            owners,probes,events,clipped=self.rows[0]['values']
            assert 1<=owners<=16 and owners<=probes<=32 and clipped in (0,1)
            windows=[r['values'] for r in self.rows if r['kind']=='window']
            ps=[r['values'] for r in self.rows if r['kind']=='probe']
            es=[r['values'] for r in self.rows if r['kind']=='event']
            assert len(windows)==owners and len(ps)==probes and len(es)==min(events,32)
            assert len(self.rows)==2+owners+probes+len(es)
            for i,(idx,role,mapped,realized,clock) in enumerate(windows):
                assert idx==i and role in (1,2,3,4) and mapped in (0,1) and realized in (0,1) and clock<probes
                assert ps[clock][2]==2
            assert windows[0][1]==1
            for i,(idx,owner,phase,hook,alive,entries,exits,depth,orphan,total,longest) in enumerate(ps):
                assert idx==i and owner<owners and phase in (1,2) and hook in (0,1) and alive in (0,1)
                assert longest<=total and orphan<=exits
            assert all((p<probes or p==32) and e in (0,1) for p,e,t in es)
            assert all(es[i][2]<=es[i+1][2] for i in range(len(es)-1))
            return {'status':'captured','records':self.rows}
        except (AssertionError, ValueError, IndexError):
            return {'status':'inconclusive','failure_category':'Incomplete or invalid bounded owner records',
                    'records':self.rows}

def image_leaf_agree(leaves, images):
    # Same GtkImage wrapper, same first wait. Complete but unrelated streams
    # must not become a captured observation. Durations intentionally differ.
    if leaves.get('status') != 'captured' or images.get('status') != 'captured':
        return True
    metrics = [r['values'] for r in leaves['records'] if r['kind'] == 'metric' and r['values'][0] == 1]
    summary = images['records'][0]['values']
    return sum(v[2] for v in metrics) == summary[6] and sum(v[3] for v in metrics) == summary[7]

def icon_image_agree(images, icons):
    # Same GtkImage wrapper; request groups must describe the same call stream.
    # Timing includes different metadata overhead and is deliberately not equal.
    if images.get('status') != 'captured' or icons.get('status') != 'captured':
        return True
    image = images['records'][0]['values']
    icon = icons['records'][0]['values']
    return image[6:8] == icon[5:7]

def stop_evidence(stopped, finish_number):
    reason=base.field(stopped[3],'reason','')
    finite=('breakpoint-hit','signal-received','exited','exited-normally','exited-signalled')
    return {'reason':reason if reason in finite else 'other',
            'finish_breakpoint_matched':reason=='breakpoint-hit' and
                base.field(stopped[3],'bkptno')==finish_number}

def collect(binary, expected, gdb='/usr/bin/gdb', timeout=180):
    binary=Path(binary).resolve(strict=True)
    if binary.name!='readiness-owner' or hashlib.sha256(binary.read_bytes()).hexdigest()!=expected:
        raise base.DiagnosticError('Diagnostic binary identity mismatch')
    report={'format':1,'acceptance_eligible':False,'diagnostic_only':True,
            'binary_sha256':expected,'status':'inconclusive','samples':[],
            'sanitizer_settings_changed':False,'leak_verdict':'not evaluated under ptrace',
            'original_first_wait_deadline_seconds':3,'raw_debugger_output_exported':False,
            'last_observation_stop':{'reason':'not-observed','finish_breakpoint_matched':False}}
    records=Records(); phases=phase_module.PhaseRecords(); leaves=leaf_module.LeafRecords(); images=image_module.ImageRecords(); icons=icon_module.IconRecords(); original=base.record
    def record(line):
        records.consume(line)
        phases.consume(line)
        leaves.consume(line)
        images.consume(line)
        icons.consume(line)
        return original(line)
    base.record=record
    ctl=None
    try:
        ctl=base.Controller([gdb,'-nx','-nh','--interpreter=mi2','-iex','set auto-load off',
                             '-iex','set debuginfod enabled off','--args',str(binary),'-p',base.CASE],timeout)
        for cmd in ('-gdb-set confirm off','-gdb-set pagination off','-gdb-set startup-with-shell off',
                    '-gdb-set disable-randomization off','-gdb-set mi-async on'):
            ctl.command(cmd)
        bp=ctl.command('-break-insert -t '+base.FUNCTION)
        b=base.field(bp[3],'bkpt',[]); number=base.field(b,'number')
        if (not isinstance(number,str) or not re.fullmatch(r'[1-9][0-9]*',number) or
            base.field(b,'addr') in ('<MULTIPLE>','<PENDING>') or base.field(b,'func')!=base.FUNCTION):
            raise base.DiagnosticError('Ambiguous wait breakpoint')
        ctl.command('-exec-run'); hit=ctl.stopped(90)
        if hit is None or base.field(hit[3],'reason')!='breakpoint-hit' or base.field(hit[3],'bkptno')!=number:
            raise base.DiagnosticError('Selected wait entry not reached')
        thread=base.field(hit[3],'thread-id')
        if not isinstance(thread,str) or not re.fullmatch(r'[1-9][0-9]{0,19}',thread):
            raise base.DiagnosticError('Missing wait thread')
        entry=ctl.command(f'-stack-list-frames --thread {thread} 0 {base.MAX_FRAMES-1}')
        report['entry_stack']=base.stack_evidence(entry[3],thread)
        if not report['entry_stack']['selected_case_frame_present']:
            raise base.DiagnosticError('Selected case identity absent')
        finish=ctl.command('-break-insert -t owner_observe_done')
        fb=base.field(finish[3],'bkpt',[])
        finish_number=base.field(fb,'number')
        if not isinstance(finish_number,str) or not re.fullmatch(r'[1-9][0-9]*',finish_number):
            raise base.DiagnosticError('Missing finish breakpoint identity')
        if base.field(fb,'func')!='owner_observe_done' or base.field(fb,'addr') in ('<MULTIPLE>','<PENDING>'):
            raise base.DiagnosticError('Ambiguous finish breakpoint')
        features=base.field(ctl.command('-list-target-features')[3],'features',[])
        if 'async' not in features: raise base.DiagnosticError('Async unavailable')
        terminal=False
        for index in range(base.MAX_SAMPLES):
            ctl.command('-exec-continue'); stopped=ctl.stopped(0.5)
            if stopped is None:
                ctl.command('-exec-interrupt --all'); stopped=ctl.stopped(10)
            if stopped is None: raise base.DiagnosticError('Interrupted target did not stop')
            report['last_observation_stop']=stop_evidence(stopped,finish_number)
            reason=base.field(stopped[3],'reason',''); sig=base.field(stopped[3],'signal-name')
            if reason.startswith('exited') or (sig and sig!='SIGINT'):
                terminal=True; break
            frames=ctl.command(f'-stack-list-frames --thread {thread} 0 {base.MAX_FRAMES-1}')
            report['samples'].append({'sample':index+1,**base.stack_evidence(frames[3],thread)})
            if records.complete: break
        # Eight half-second samples are insufficient for known6s dispatch. Allow
        # the SAME child to finish first wait, within the SAME180s controller cap.
        # No second child or retry, no deadline/predicate change in tested code.
        if not records.complete and not terminal:
            ctl.command('-exec-continue')
            while not records.complete:
                stopped=ctl.stopped(max(0.01,ctl.deadline-base.time.monotonic()))
                if stopped is not None:
                    report['last_observation_stop']=stop_evidence(stopped,finish_number)
                    break
        report['owner_observation']=records.result()
        report['phase_observation']=phases.result()
        report['leaf_observation']=leaves.result()
        report['image_observation']=images.result()
        report['icon_observation']=icons.result()
        if not image_leaf_agree(report['leaf_observation'], report['image_observation']):
            report['image_observation']['status']='inconclusive'
            report['image_observation']['failure_category']='Image and leaf call counts disagree'
        if not icon_image_agree(report['image_observation'], report['icon_observation']):
            report['icon_observation']['status']='inconclusive'
            report['icon_observation']['failure_category']='Icon and image call counts disagree'
        report['status']='captured' if all(report[k]['status']=='captured' for k in ('owner_observation','phase_observation','leaf_observation','image_observation','icon_observation')) else 'inconclusive'
    except (base.DiagnosticError,OSError,ValueError,TypeError,UnicodeError) as error:
        report['failure_category']=str(error) if isinstance(error,base.DiagnosticError) else type(error).__name__
        report['owner_observation']=records.result()
    finally:
        report['phase_observation']=phases.result()
        report['leaf_observation']=leaves.result()
        if 'image_observation' not in report: report['image_observation']=images.result()
        if 'icon_observation' not in report: report['icon_observation']=icons.result()
        try:
            if ctl is not None:
                try:
                    report['owned_child_cleanup_completed']=ctl.cleanup()
                except (base.DiagnosticError,OSError):
                    report['owned_child_cleanup_completed']=False
                report['cleanup_evidence']=getattr(ctl,'cleanup_evidence',{'stage':'unavailable'})
                if not report['owned_child_cleanup_completed']: report['status']='inconclusive'
        finally: base.record=original
    report['icon_observation']['image_snapshot_status']=report['image_observation']['status']
    report['icon_observation']['image_snapshot_identity_coverage_complete']=report['image_observation'].get('identity_coverage_complete',False)
    report['icon_observation']['tracked_scope']='Live weak snapshot membership only; image_observation flags and semantic/type coverage also govern ancestor/host interpretation'
    return report

if __name__=='__main__':
    parser=argparse.ArgumentParser()
    for flag in ('binary','sha256','output'): parser.add_argument('--'+flag,required=True)
    args=parser.parse_args()
    report=collect(args.binary,args.sha256)
    base.write_report(args.output,report)
    sys.exit(0 if report['status']=='captured' else 2)
