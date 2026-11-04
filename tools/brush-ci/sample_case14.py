#!/usr/bin/env python3
"""Bounded, non-acceptance GDB/MI sampling of one explicitly identified child.
Never attach; never change ASan/LSan flags; never export raw MI or values.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import selectors
import signal
import subprocess
import time

CASE = '/brush-thumbnail-native-popup/private-target-release'
FUNCTION = 'transform_popup_wait_stable_geometry_observed'
MAX_WIRE = 2 * 1024 * 1024
MAX_LINE = 65536
MAX_FRAMES = 96
MAX_SAMPLES = 8
MAX_REPORT_BYTES = 1024 * 1024

class DiagnosticError(Exception):
    pass

class MI:
    """Small bounded MI result parser. No evaluation of inferior expressions."""
    def __init__(self, text):
        if len(text) > MAX_LINE: raise DiagnosticError('MI record exceeds bound')
        self.s, self.i = text, 0
    def value(self, depth=0):
        if depth > 16: raise DiagnosticError('MI nesting exceeds bound')
        if self.i >= len(self.s): raise DiagnosticError('Missing MI value')
        c = self.s[self.i]
        if c == '"':
            # MI strings use C escapes; requested fields normally use JSON subset.
            # Reject unsupported escape syntax rather than interpreting it.
            try: v, n = json.JSONDecoder().raw_decode(self.s[self.i:])
            except ValueError: raise DiagnosticError('Unsupported MI string encoding')
            self.i += n
            return v
        if c in '{[':
            end = '}' if c == '{' else ']'
            self.i += 1
            out = []
            while self.i < len(self.s) and self.s[self.i] != end:
                m = re.match(r'[A-Za-z_][A-Za-z0-9_-]*=', self.s[self.i:])
                if m:
                    key = m.group()[:-1]; self.i += len(m.group())
                    out.append((key, self.value(depth + 1)))
                else: out.append(self.value(depth + 1))
                if self.i < len(self.s) and self.s[self.i] == ',': self.i += 1
                elif self.i < len(self.s) and self.s[self.i] != end:
                    raise DiagnosticError('Malformed MI container')
            if self.i >= len(self.s): raise DiagnosticError('Unclosed MI container')
            self.i += 1
            return out
        raise DiagnosticError('Unsupported MI value')
    def fields(self):
        result = []
        while self.i < len(self.s):
            m = re.match(r'([A-Za-z_][A-Za-z0-9_-]*)=', self.s[self.i:])
            if not m: raise DiagnosticError('Malformed MI field')
            self.i += len(m.group()); result.append((m.group(1), self.value()))
            if self.i == len(self.s): break
            if self.s[self.i] != ',': raise DiagnosticError('Malformed MI separator')
            self.i += 1
        return result

def field(fields, key, default=None):
    if not isinstance(fields, list): raise DiagnosticError('Malformed MI field container')
    vals = [v for item in fields if isinstance(item, tuple) and len(item) == 2
            for k, v in [item] if k == key]
    if len(vals) > 1: raise DiagnosticError('Duplicate MI identity field')
    return vals[0] if vals else default

def record(line):
    m = re.match(r'^(\d*)([\^*=])([a-zA-Z0-9_-]+)(?:,(.*))?$', line)
    if not m: return None  # Stream output deliberately discarded, not exported.
    return (m.group(1), m.group(2), m.group(3), MI(m.group(4) or '').fields())

def safe_text(value, basename=False):
    if not isinstance(value, str): return None
    if basename: value = value.replace('\\', '/').rsplit('/', 1)[-1]
    if len(value) > 180 or re.search(r'0x[0-9a-fA-F]+|[\x00-\x1f\x7f]', value): return None
    if not re.fullmatch(r'[A-Za-z0-9_.$:+~<>(), *?/@\[\]-]+', value): return None
    return value

def safe_stack(fields, thread):
    if not isinstance(thread, str) or not re.fullmatch(r'[1-9][0-9]{0,19}', thread):
        raise DiagnosticError('Invalid bounded thread identity')
    frames = field(fields, 'stack', [])
    if not isinstance(frames, list) or len(frames) > MAX_FRAMES:
        raise DiagnosticError('Stack bound exceeded')
    out = []
    for pair in frames:
        if not isinstance(pair, tuple) or pair[0] != 'frame': raise DiagnosticError('Invalid stack record')
        frame = pair[1]
        row = {'thread_id': thread}
        func = safe_text(field(frame, 'func'))
        filename = safe_text(field(frame, 'file'), basename=True)
        line = field(frame, 'line', '')
        if func: row['function'] = func
        if filename: row['file'] = filename
        if isinstance(line, str) and re.fullmatch(r'[0-9]{1,8}', line): row['line'] = int(line)
        out.append(row)
    return out

def stack_evidence(fields, thread):
    frames = safe_stack(fields, thread)
    names = {f.get('function') for f in frames}
    return {'frames': frames, 'frame_limit': MAX_FRAMES,
            'truncation_possible': len(frames) == MAX_FRAMES,
            'geometry_wait_frame_present': FUNCTION in names,
            'brush_open_frame_present': 'brush_popup_open' in names,
            'selected_case_frame_present': 'brush_popup_private_target_release' in names,
            'same_wait_invocation_as_entry_proven': False}

class Controller:
    def __init__(self, argv, timeout=180):
        self.deadline = time.monotonic() + timeout
        self.p = subprocess.Popen(argv, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                  stderr=subprocess.STDOUT, start_new_session=True)
        self.selector = None
        try:
            self.selector = selectors.DefaultSelector()
            self.selector.register(self.p.stdout, selectors.EVENT_READ)
        except OSError:
            try:
                self.p.kill(); self.p.wait(timeout=5)
            except (OSError, subprocess.TimeoutExpired):
                raise DiagnosticError('Debugger setup cleanup incomplete')
            finally:
                if self.selector is not None: self.selector.close()
                for stream in (self.p.stdin, self.p.stdout):
                    if stream: stream.close()
            raise
        self.buffer = b''; self.total = 0; self.token = 0; self.queue = []
        self.inferior = None; self.pidfd = None; self.running = False; self.run_requested = False
        self.unknown_inferior = False
    def own_inferior(self, value):
        if self.inferior is not None:
            self.unknown_inferior = True
            raise DiagnosticError('Unexpected additional inferior')
        if not isinstance(value, str) or not re.fullmatch(r'[1-9][0-9]{0,9}', value):
            raise DiagnosticError('Invalid inferior identity')
        pid = int(value)
        # No environment/memory reads. Verify GDB is the direct parent before
        # retaining a race-safe descriptor to this particular process.
        stat = Path(f'/proc/{pid}/stat').read_text()
        tail = stat.rsplit(')', 1)[1].split()
        if int(tail[1]) != self.p.pid: raise DiagnosticError('Inferior not debugger-owned child')
        fd = os.pidfd_open(pid)
        try:
            stat2 = Path(f'/proc/{pid}/stat').read_text()
            tail2 = stat2.rsplit(')', 1)[1].split()
            if int(tail2[1]) != self.p.pid or tail[19] != tail2[19]:
                raise DiagnosticError('Inferior identity changed')
            # Retain authority to signal only after both identity checks pass.
            self.pidfd = fd
            self.inferior = pid
            fd = None
        finally:
            if fd is not None: os.close(fd)
    def observe(self, rec):
        if rec[1:3] == ('=', 'thread-group-started'):
            self.own_inferior(field(rec[3], 'pid'))
        if rec[1:3] == ('*', 'running'): self.running = True
        if rec[1:3] == ('*', 'stopped'): self.running = False
    def next(self, until=None):
        limit = min(self.deadline, until or self.deadline)
        while time.monotonic() < limit:
            if b'\n' in self.buffer:
                line, self.buffer = self.buffer.split(b'\n', 1)
                if len(line) > MAX_LINE: raise DiagnosticError('MI line bound exceeded')
                rec = record(line.decode('utf-8', errors='strict').rstrip('\r'))
                if rec: self.observe(rec); return rec
                continue
            if len(self.buffer) > MAX_LINE: raise DiagnosticError('MI line bound exceeded')
            ready = self.selector.select(max(0, limit - time.monotonic()))
            if not ready: return None
            chunk = os.read(self.p.stdout.fileno(), 16384)
            if not chunk: raise DiagnosticError('Debugger stream ended')
            self.total += len(chunk)
            if self.total > MAX_WIRE: raise DiagnosticError('Debugger output bound exceeded')
            self.buffer += chunk
        if time.monotonic() >= self.deadline: raise DiagnosticError('Diagnostic timeout')
        return None
    def command(self, command, seconds=10):
        if command == "-exec-run": self.run_requested = True
        self.token += 1; token = str(self.token)
        self.p.stdin.write((token + command + '\n').encode()); self.p.stdin.flush()
        end = min(self.deadline, time.monotonic() + seconds)
        while time.monotonic() < end:
            rec = self.next(end)
            if rec is None: break
            if rec[0] == token and rec[1] == '^':
                if rec[2] == 'error': raise DiagnosticError('Debugger rejected command')
                return rec
            self.queue.append(rec)
            if len(self.queue) > 2048: raise DiagnosticError('Event bound exceeded')
        raise DiagnosticError('Debugger command timeout')
    def stopped(self, seconds):
        end = min(self.deadline, time.monotonic() + seconds)
        while time.monotonic() < end:
            if self.queue: rec = self.queue.pop(0)
            else: rec = self.next(end)
            if rec is None: return None
            if rec[1:3] == ('*', 'stopped'): return rec
        return None
    def cleanup(self):
        # Retain the verified identity through debugger shutdown. Debugger exit
        # alone is never child-death proof: require final pidfd readiness too.
        evidence = self.cleanup_evidence = {
            'stage': 'start', 'run_requested': self.run_requested,
            'owned_pidfd_present': self.pidfd is not None,
            'unknown_inferior': self.unknown_inferior,
            'pidfd_signal': 'not-attempted', 'pidfd_wait': 'not-attempted',
            'pidfd_final_wait': 'not-attempted', 'pidfd_close': 'not-attempted', 'debugger_poll': 'not-attempted', 'debugger_exit': 'not-attempted',
            'debugger_grace_wait': 'not-attempted', 'debugger_kill': 'not-attempted',
            'debugger_final_wait': 'not-attempted', 'stream_close': 'not-attempted'}
        child_done = not self.run_requested
        debugger_done = False
        if self.pidfd is not None:
            try:
                evidence['stage'] = 'pidfd-signal'
                try:
                    signal.pidfd_send_signal(self.pidfd, signal.SIGKILL)
                    evidence['pidfd_signal'] = 'sent'
                except ProcessLookupError:
                    evidence['pidfd_signal'] = 'already-exited'
                evidence['stage'] = 'pidfd-wait'
                with selectors.DefaultSelector() as ready:
                    ready.register(self.pidfd, selectors.EVENT_READ)
                    child_done = bool(ready.select(2))
                    evidence['pidfd_wait'] = 'observed' if child_done else 'timeout'
            except Exception:
                # Unexpected selector/setup failures must not skip debugger
                # shutdown or release of the retained verified descriptor.
                if evidence['stage'] == 'pidfd-signal': evidence['pidfd_signal'] = 'error'
                else: evidence['pidfd_wait'] = 'error'
                child_done = False
        try:
            evidence['stage'] = 'debugger-poll'
            polled = self.p.poll()
            evidence['debugger_poll'] = 'running' if polled is None else 'exited'
            if polled is None:
                # Orderly GDB exit is unchanged, after the initial pidfd wait.
                try:
                    evidence['stage'] = 'debugger-exit-request'
                    self.p.stdin.write(b'-gdb-exit\n'); self.p.stdin.flush()
                    evidence['debugger_exit'] = 'requested'
                    evidence['stage'] = 'debugger-grace-wait'
                    self.p.wait(timeout=2)
                    evidence['debugger_grace_wait'] = 'observed'
                except (OSError, subprocess.TimeoutExpired) as error:
                    if evidence['stage'] == 'debugger-exit-request': evidence['debugger_exit'] = 'error'
                    else: evidence['debugger_grace_wait'] = 'timeout' if isinstance(error, subprocess.TimeoutExpired) else 'error'
                    evidence['stage'] = 'debugger-kill'
                    self.p.kill()
                    evidence['debugger_kill'] = 'sent'
            else:
                evidence['debugger_exit'] = 'already-exited'
            evidence['stage'] = 'debugger-final-wait'
            self.p.wait(timeout=3)
            evidence['debugger_final_wait'] = 'observed'
            debugger_done = True
        except (OSError, subprocess.TimeoutExpired) as error:
            if evidence['stage'] == 'debugger-poll': evidence['debugger_poll'] = 'error'
            elif evidence['stage'] == 'debugger-kill': evidence['debugger_kill'] = 'error'
            else: evidence['debugger_final_wait'] = 'timeout' if isinstance(error, subprocess.TimeoutExpired) else 'error'
            debugger_done = False
        finally:
            if self.pidfd is not None:
                try:
                    evidence['stage'] = 'pidfd-final-wait'
                    with selectors.DefaultSelector() as ready:
                        ready.register(self.pidfd, selectors.EVENT_READ)
                        child_done = bool(ready.select(2))
                        evidence['pidfd_final_wait'] = 'observed' if child_done else 'timeout'
                except Exception:
                    # Keep error metadata finite; always close the pidfd.
                    evidence['pidfd_final_wait'] = 'error'
                    child_done = False
                finally:
                    evidence['stage'] = 'pidfd-close'
                    try:
                        os.close(self.pidfd)
                        evidence['pidfd_close'] = 'closed'
                    except OSError:
                        evidence['pidfd_close'] = 'error'
                        child_done = False
                    self.pidfd = None
            evidence['child_termination_observed'] = child_done
            evidence['debugger_termination_observed'] = debugger_done
            evidence['stage'] = 'stream-close'
            self.selector.close()
            for stream in (self.p.stdin, self.p.stdout):
                if stream: stream.close()
            evidence['stream_close'] = 'closed'
        evidence['stage'] = 'complete'
        return child_done and debugger_done and not self.unknown_inferior

def sample(gdb, binary, args, expected, timeout=180):
    binary = Path(binary).resolve(strict=True)
    if hashlib.sha256(binary.read_bytes()).hexdigest() != expected:
        raise DiagnosticError('Binary hash mismatch')
    if args != ['-p', CASE]:
        raise DiagnosticError('Wrong exact selected case arguments')
    report = {'format': 1, 'acceptance_eligible': False, 'scope': 'first selected-case wait-entry thread stack diagnostic only',
              'binary_sha256': expected, 'case': CASE, 'samples': [], 'status': 'inconclusive',
              'leak_verdict': 'not evaluated under ptrace', 'sanitizer_settings_changed': False,
              'raw_debugger_output_exported': False}
    try:
        ctl = Controller([gdb, '-nx', '-nh', '--interpreter=mi2',
                          '-iex', 'set auto-load off', '-iex', 'set debuginfod enabled off',
                          '--args', str(binary), *args], timeout)
    except (OSError, DiagnosticError) as e:
        report['failure_category'] = 'Debugger launch/setup failed'
        report['owned_child_cleanup_completed'] = not isinstance(e, DiagnosticError)
        return report
    try:
        for cmd in ('-gdb-set confirm off', '-gdb-set pagination off',
                    '-gdb-set startup-with-shell off', '-gdb-set disable-randomization off',
                    '-gdb-set mi-async on'):
            ctl.command(cmd)
        bp = ctl.command('-break-insert -t ' + FUNCTION)
        b = field(bp[3], 'bkpt', [])
        number = field(b, 'number')
        if not isinstance(number, str) or not re.fullmatch(r'[1-9][0-9]*', number):
            raise DiagnosticError('Ambiguous breakpoint identity')
        if field(b, 'addr') in ('<MULTIPLE>', '<PENDING>') or field(b, 'func') != FUNCTION:
            raise DiagnosticError('Breakpoint is not exact resolved function')
        ctl.command('-exec-run')
        hit = ctl.stopped(90)
        if hit is None or field(hit[3], 'reason') != 'breakpoint-hit' or field(hit[3], 'bkptno') != number:
            raise DiagnosticError('Selected wait entry was not reached')
        thread = field(hit[3], 'thread-id')
        if not isinstance(thread, str) or not re.fullmatch(r'[1-9][0-9]{0,19}', thread):
            raise DiagnosticError('Missing breakpoint thread identity')
        entry = ctl.command(f'-stack-list-frames --thread {thread} 0 {MAX_FRAMES-1}')
        report['entry_stack'] = stack_evidence(entry[3], thread)
        features = field(ctl.command('-list-target-features')[3], 'features', [])
        if 'async' not in features: raise DiagnosticError('Async debugging unavailable')
        for index in range(MAX_SAMPLES):
            ctl.command('-exec-continue')
            started = time.monotonic()
            stopped = ctl.stopped(0.5)
            if stopped is None:
                ctl.command('-exec-interrupt --all')
                stopped = ctl.stopped(10)
            if stopped is None: raise DiagnosticError('Interrupted target did not stop')
            reason = field(stopped[3], 'reason', '')
            if reason.startswith('exited'):
                report['termination'] = 'inferior exited before sampling completed'; break
            frames = ctl.command(f'-stack-list-frames --thread {thread} 0 {MAX_FRAMES-1}')
            report['samples'].append({'sample': index + 1,
                                      'sample_wall_interval_ms': round((time.monotonic()-started)*1000),
                                      'stop_reason': reason if reason in ('signal-received','breakpoint-hit','end-stepping-range') else 'other',
                                      **stack_evidence(frames[3], thread)})
            sig = field(stopped[3], 'signal-name')
            if sig and sig != 'SIGINT':
                report['termination'] = safe_text(sig) or 'other signal'; break
        report['status'] = 'captured' if report['samples'] else 'inconclusive'
    except (DiagnosticError, OSError, ValueError, UnicodeError, TypeError) as e:
        # Exception text comes only from fixed diagnostics; never raw MI messages.
        report['failure_category'] = str(e) if isinstance(e, DiagnosticError) else type(e).__name__
    finally:
        try:
            report['owned_child_cleanup_completed'] = ctl.cleanup()
            report['cleanup_evidence'] = ctl.cleanup_evidence
            if not report['owned_child_cleanup_completed']:
                report['status'] = 'inconclusive'
                report['failure_category'] = 'Cleanup incomplete'
        except (DiagnosticError, OSError):
            report['cleanup_evidence'] = getattr(ctl, 'cleanup_evidence', {'stage': 'unavailable'})
            report['owned_child_cleanup_completed'] = False
            report['status'] = 'inconclusive'
            report['failure_category'] = 'Cleanup incomplete'
    return report

def write_report(output, report):
    encoded = json.dumps(report, indent=2) + '\n'
    if len(encoded.encode('utf-8')) > MAX_REPORT_BYTES:
        report = {'format': 1, 'acceptance_eligible': False,
                  'status': 'inconclusive', 'failure_category': 'Sanitized report size cap exceeded',
                  'samples': [], 'owned_child_cleanup_completed': report.get('owned_child_cleanup_completed') is True,
                  'leak_verdict': 'not evaluated under ptrace',
                  'sanitizer_settings_changed': False, 'raw_debugger_output_exported': False}
        encoded = json.dumps(report, indent=2) + '\n'
    Path(output).write_text(encoded)
    return report

def main():
    p = argparse.ArgumentParser(); p.add_argument('--gdb', default='gdb')
    p.add_argument('--binary', required=True); p.add_argument('--sha256', required=True)
    p.add_argument('--output', required=True); p.add_argument('args', nargs=argparse.REMAINDER)
    a = p.parse_args()
    args = a.args[1:] if a.args[:1] == ['--'] else a.args
    try:
        report = sample(a.gdb, a.binary, args, a.sha256)
    except (DiagnosticError, OSError, ValueError):
        report = {'format': 1, 'acceptance_eligible': False, 'status': 'inconclusive',
                  'failure_category': 'Input identity validation failed', 'samples': []}
    report = write_report(a.output, report)
    return 0 if report['status'] == 'captured' else 2

if __name__ == '__main__': raise SystemExit(main())
