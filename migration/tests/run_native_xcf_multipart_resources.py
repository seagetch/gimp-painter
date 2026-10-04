#!/usr/bin/env python3
"""Native >256 MiB Save/Open proof, run under the shared build lock (no GUI).

The child uses a private disk-backed TMPDIR. Linux RSS and open temporary file
allocation are sampled; these figures include the host and are not an allocator
or cross-platform bounded-RSS proof. Mapping-only source files with no open FD
are excluded from FD allocation totals and are reported explicitly.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import resource
import subprocess
import tempfile
import time

p = argparse.ArgumentParser()
p.add_argument('build', type=Path)
p.add_argument('--report', type=Path, required=True)
p.add_argument('--payload-mib', type=int, choices=[256, 512, 1024, 2048], default=256)
a = p.parse_args()
root = Path(__file__).resolve().parents[2]
build = a.build.resolve()
exe = build / 'app/tests/painter-xcf-roundtrip'
source_paths = ['app/xcf/painter-xcf-storage.cpp', 'app/xcf/painter-xcf-storage.hpp',
                'app/xcf/painter-xcf-multipart.cpp', 'app/xcf/painter-xcf-multipart.hpp',
                'app/xcf/painter-xcf-transport.cpp', 'app/xcf/painter-xcf-preserve.cpp', 'app/xcf/painter-xcf-load.cpp',
                'app/xcf/painter-xcf-preserve.h', 'app/xcf/painter-xcf-arguments.cpp',
                'app/xcf/xcf-load.c', 'app/xcf/xcf-save.c', 'app/xcf/xcf.c', 'app/xcf/xcf.h',
                'app/painter/bytes.hpp', 'app/core/gimp-painter-provenance.cpp',
                'app/core/gimp-painter-provenance.h', 'app/tests/test-painter-xcf-roundtrip.c',
                'app/tests/test-painter-xcf-multipart.inc', 'app/tests/test-painter-xcf-multipart-adversarial.inc',
                'app/core/gimpfilterlayer.cpp', 'app/core/gimpfilterlayer.h', 'app/core/gimpfilterlayer-arguments.hpp',
                'app/tests/test-painter-xcf-argument-resources.cpp', 'app/xcf/painter-xcf-arguments.hpp',
                'migration/tests/run_native_xcf_multipart_resources.py']
def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()
hashes = {name: sha(root / name) for name in source_paths}
report = {'scope': 'Ordinary public PDB Save/Open with multipart metadata, Clone references and Filter raw/typed state',
          'sources_sha256': hashes, 'executable_sha256': sha(exe), 'sample_interval_ms': 10,
          'payload_bytes': a.payload_mib * 1024 * 1024 + 1,
          'limits': ['Linux host RSS includes GIMP, libraries, retained source/capsules and mapped pages',
                     'FD allocation excludes mappings whose source descriptor was already closed',
                     'No Windows/macOS or big-endian proof', 'Native API transaction evidence; no interactive GUI latency or platform claim']}
env = dict(os.environ)
env.update(GIMP_TESTING_ABS_TOP_SRCDIR=str(root), GIMP_TESTING_ABS_TOP_BUILDDIR=str(build),
           GIMP_TESTING_PLUGINDIRS=str(build / 'plug-ins/common'), UI_TEST='yes', GSETTINGS_BACKEND='memory',
           GIMP_TEST_XCF_MULTIPART_MIB=str(a.payload_mib))
command = [str(exe), '-m', 'slow', '-p', '/painter-xcf-multipart/native-large']
report['command'] = command
stdout_path = build / f'xcf-resource-{a.payload_mib}.stdout.log'
stderr_path = build / f'xcf-resource-{a.payload_mib}.stderr.log'
peak_rss = peak_fd_bytes = peak_fd_allocated = peak_fds = 0
rss_at_peak = {}
peak_mapping_bytes = peak_mapping_count = peak_filesystem_allocated_delta = 0
peak_mapped_allocated = 0
mapping_allocation_stat_denials = 0
samples = 0
start = time.monotonic()
with tempfile.TemporaryDirectory(prefix='xcf-multipart-resource-', dir=build) as tmp:
    env['TMPDIR'] = tmp
    fs_before = os.statvfs(tmp)
    fs_free_before = fs_before.f_bfree * fs_before.f_frsize
    with stdout_path.open('w') as stdout, stderr_path.open('w') as stderr:
        child = subprocess.Popen(command, cwd=build, env=env, stdout=stdout, stderr=stderr)
        while child.poll() is None:
            try:
                status = Path(f'/proc/{child.pid}/status').read_text()
                rss = next((int(line.split()[1]) * 1024 for line in status.splitlines() if line.startswith('VmRSS:')), 0)
                if rss > peak_rss:
                    rss_at_peak = {line.split(':', 1)[0]: int(line.split()[1]) * 1024
                                   for line in status.splitlines() if line.startswith(('RssAnon:', 'RssFile:', 'RssShmem:'))}
                peak_rss = max(peak_rss, rss)
                allocated = {}
                for fd in Path(f'/proc/{child.pid}/fd').iterdir():
                    try:
                        name = os.readlink(fd)
                        if name.startswith(tmp + '/'):
                            st = fd.stat()
                            allocated[(st.st_dev, st.st_ino)] = (st.st_size, st.st_blocks * 512)
                    except (FileNotFoundError, PermissionError, ProcessLookupError):
                        pass
                fd_count = len(allocated)
                fd_logical = sum(v[0] for v in allocated.values())
                fd_allocated = sum(v[1] for v in allocated.values())
                mapped = {}
                for line in Path(f'/proc/{child.pid}/maps').read_text().splitlines():
                    fields = line.split(None, 5)
                    if len(fields) == 6 and fields[5].startswith(tmp + '/'):
                        address = fields[0]
                        begin, end = (int(value, 16) for value in address.split('-'))
                        mapped[(fields[3], fields[4])] = end - begin
                        try:
                            st = Path(f'/proc/{child.pid}/map_files/{address}').stat()
                            allocated[(st.st_dev, st.st_ino)] = (st.st_size, st.st_blocks * 512)
                        except (PermissionError, FileNotFoundError, ProcessLookupError):
                            mapping_allocation_stat_denials += 1
                peak_mapping_bytes = max(peak_mapping_bytes, sum(mapped.values()))
                peak_mapping_count = max(peak_mapping_count, len(mapped))
                peak_mapped_allocated = max(peak_mapped_allocated, sum(v[1] for v in allocated.values()))
                fs_now = os.statvfs(tmp)
                peak_filesystem_allocated_delta = max(peak_filesystem_allocated_delta,
                    fs_free_before - fs_now.f_bfree * fs_now.f_frsize)
                peak_fds = max(peak_fds, fd_count)
                peak_fd_bytes = max(peak_fd_bytes, fd_logical)
                peak_fd_allocated = max(peak_fd_allocated, fd_allocated)
                samples += 1
            except (FileNotFoundError, PermissionError, ProcessLookupError):
                pass
            time.sleep(.01)
        exit_code = child.wait()
    remaining = sorted([{'path': str(item.relative_to(tmp)), 'is_dir': item.is_dir(), 'size': item.stat().st_size} for item in Path(tmp).rglob('*')], key=lambda x: x['path'])
report.update(exit_code=exit_code, elapsed_seconds=time.monotonic() - start,
              sampled_peak_rss_bytes=peak_rss, wait4_peak_rss_bytes=resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss * 1024,
              rss_components_at_sampled_peak=rss_at_peak,
              sampled_peak_temp_fd_count=peak_fds, sampled_peak_temp_fd_logical_bytes=peak_fd_bytes,
              sampled_peak_temp_fd_allocated_bytes=peak_fd_allocated,
              sampled_peak_temp_mapping_bytes=peak_mapping_bytes,
              sampled_peak_temp_mapping_count=peak_mapping_count,
              sampled_peak_fd_or_accessible_mapping_allocated_bytes=peak_mapped_allocated,
              mapping_allocation_stat_denials=mapping_allocation_stat_denials,
              sampled_peak_filesystem_allocated_delta_bytes=peak_filesystem_allocated_delta,
              filesystem_measurement_scope='Actual filesystem free-block delta, includes concurrently allocated non-task files on this filesystem; mapping logical bytes are not physical allocation',
              temp_directory_entries_after_exit=remaining, samples=samples,
              stdout_summary=[line for line in stdout_path.read_text().splitlines()
                              if line.startswith(('ok ', 'not ok ', '1..', '# multipart public-pdb payload='))],
              stdout_sha256=sha(stdout_path), stderr_sha256=sha(stderr_path),
              diagnostics=[line for line in stderr_path.read_text().splitlines()
                           if any(word in line for word in ['ERROR:', 'runtime error:', 'AddressSanitizer', 'assertion'])],
              changed_during_run=[n for n,h in hashes.items() if sha(root/n) != h])
a.report.write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps({k:v for k,v in report.items() if k not in ['sources_sha256','command']}, indent=2))
transport_leftovers = [entry for entry in remaining if Path(entry['path']).name.startswith(('gimp-xcf-', 'painter-large-record-', 'painter-roundtrip-'))]
report['transport_leftovers_after_exit'] = transport_leftovers
a.report.write_text(json.dumps(report, indent=2) + '\n')
raise SystemExit(exit_code or bool(transport_leftovers) or bool(report['changed_during_run']))
