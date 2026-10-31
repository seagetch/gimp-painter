"""Fail-closed, pure validators; no sanitizer execution on import."""
import re


def check_run(mode, code, stdout, stderr):
    if stdout.count('babl_init completed\n') != 1 or stdout.count('babl_exit completed\n') != 1:
        raise ValueError('incomplete lifecycle')
    if any(line not in ('babl_init completed', 'babl_exit completed') and not line.startswith('LOADED\t') for line in stdout.splitlines()):
        raise ValueError('unexpected lifecycle stdout')
    forbidden = ('ERROR: AddressSanitizer:', 'runtime error:', 'DEADLYSIGNAL',
                 'LeakSanitizer has encountered a fatal error', 'Suppressions used',
                 'ERROR: UndefinedBehaviorSanitizer', 'failed to intercept')
    if any(token in stderr for token in forbidden):
        raise ValueError('non-leak sanitizer error or suppression')
    if mode == 'patched':
        if code != 0 or stderr.strip():
            raise ValueError('GREEN requires exit zero and empty stderr')
        return {'leaked_bytes': 0, 'allocations': 0}
    if mode != 'original' or code != 23:
        raise ValueError('RED requires original and exact LSan exit 23')
    if stderr.count('ERROR: LeakSanitizer: detected memory leaks') != 1:
        raise ValueError('missing/duplicate LSan report')
    summary = re.findall(r'SUMMARY: AddressSanitizer: (\d+) byte\(s\) leaked in (\d+) allocation\(s\)\.', stderr)
    if len(summary) != 1 or not all(int(x) > 0 for x in summary[0]):
        raise ValueError('missing/invalid leak summary')
    blocks = re.findall(r'(?:Direct|Indirect) leak of (\d+) byte\(s\) in (\d+) object\(s\) allocated from:\n(.*?)(?=\n(?:Direct|Indirect) leak|\nSUMMARY:)', stderr, re.S)
    if not blocks or any('babl_format_new' not in b[2] or 'babl_strdup' not in b[2] or not re.search(r'babl-format\.c:377(?:\D|$)', b[2]) for b in blocks):
        raise ValueError('leak outside expected owned format doc allocation path')
    for _, _, stack in blocks:
        frames = [line for line in stack.splitlines() if line.strip()]
        # Sanitizer frames have a sequential index and an instruction address.
        # Do not discard arbitrary diagnostics hidden between valid frames.
        for index, line in enumerate(frames):
            if not re.fullmatch(r'\s+#' + str(index) + r'\s+0x[0-9a-fA-F]+\s+.+', line):
                raise ValueError('unexpected content in leak stack')
    if tuple(sum(int(b[i]) for b in blocks) for i in (0, 1)) != tuple(map(int, summary[0])):
        raise ValueError('unaccounted leak totals')
    # Reject extra diagnostics outside the structured leak report.
    rest = re.sub(r'==\d+==ERROR: LeakSanitizer: detected memory leaks', '', stderr)
    rest = re.sub(r'(?:Direct|Indirect) leak of \d+ byte\(s\) in \d+ object\(s\) allocated from:\n.*?(?=\n(?:Direct|Indirect) leak|\nSUMMARY:)', '', rest, flags=re.S)
    rest = re.sub(r'SUMMARY: AddressSanitizer: \d+ byte\(s\) leaked in \d+ allocation\(s\)\.', '', rest)
    if rest.strip('=\n \t'):
        raise ValueError('unexpected diagnostic outside leak report')
    return dict(zip(('leaked_bytes', 'allocations'), map(int, summary[0])))


def loaded_paths(stdout):
    paths = [line.split('\t', 1)[1] for line in stdout.splitlines() if line.startswith('LOADED\t')]
    if len(paths) != len(set(paths)):
        raise ValueError('duplicate runtime object')
    return paths


def check_elf(dynamic, program):
    if '(RPATH)' in dynamic or '(RUNPATH)' in dynamic:
        raise ValueError('RPATH/RUNPATH forbidden')
    if 'BIND_NOW' not in dynamic and not re.search(r'FLAGS_1.*\bNOW\b', dynamic):
        raise ValueError('BIND_NOW missing')
    if 'GNU_RELRO' not in program:
        raise ValueError('RELRO missing')
