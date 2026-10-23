#!/usr/bin/env python3
"""Extract pinned Debian build dependencies locally, without installing packages.

This helper is for an amd64 Debian 13 host that already has Python 3, a C/C++
compiler, apt-get, dpkg-deb, pkg-config, and the Debian archive keyring. It never
uses sudo, modifies system APT configuration, or runs package maintainer scripts.
"""

from __future__ import annotations

import argparse
import concurrent.futures
import getpass
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import subprocess


REPO = Path(__file__).resolve().parent.parent
DEFAULT_LOCK = REPO / "migration/baseline/debian13-package-lock.json"
SYSTEM_PREFIX = re.compile(r"(?:(?<![\w/.-])|(?<=-L)|(?<=-I))/usr(?=/|\s|$)")


def run(argv: list[str], env: dict[str, str], log: Path) -> None:
    with log.open("w") as output:
        result = subprocess.run(argv, env=env, stdout=output,
                                stderr=subprocess.STDOUT, check=False)
    if result.returncode:
        raise RuntimeError(f"Command failed ({result.returncode}); see {log}")


def relocate(root: Path) -> None:
    # pkgconf --define-prefix incorrectly derives /usr/lib for Debian's
    # multiarch .pc directory. Rewrite only unrelocated /usr path tokens.
    for pc in (root / "usr").rglob("*.pc"):
        if pc.is_file():
            pc.write_text(SYSTEM_PREFIX.sub(str(root / "usr"), pc.read_text()))
    # Debian's architecture wrappers use absolute native tool paths. Keep the
    # host Python shebang, relocating only tool/data paths in their bodies.
    scripts = list((root / "usr/bin").glob("x86_64-linux-gnu-g-ir-*"))
    scripts += [root / "usr/bin/g-ir-scanner"]
    for script in scripts:
        if script.exists():
            first, rest = script.read_text().split("\n", 1)
            script.write_text(first + "\n" +
                              SYSTEM_PREFIX.sub(str(root / "usr"), rest))
    # Debian's lcms2.pc lists two static plug-in archives in Libs. Meson passes
    # those -l flags to g-ir-scanner as libraries to dlopen, which cannot work.
    # Keep them on the scanner's real link line as --extra-library instead.
    # Application/library linker flags and the package's .pc contract stay intact.
    scanner = root / "usr/bin/x86_64-linux-gnu-g-ir-scanner"
    if scanner.exists():
        static_names = ("lcms2_fast_float", "lcms2_threaded")
        libdir = root / "usr/lib/x86_64-linux-gnu"
        if all((libdir / ("lib" + name + ".a")).is_file() and
               not (libdir / ("lib" + name + ".so")).exists()
               for name in static_names):
            old = "argv = [TOOL_PATH] + extra_argv + sys.argv[1:]"
            new = ("argv = [TOOL_PATH] + extra_argv + ["
                   "('--extra-library=' + arg[2:]) if arg in "
                   "('-llcms2_fast_float', '-llcms2_threaded') else arg "
                   "for arg in sys.argv[1:]]")
            text = scanner.read_text()
            if old not in text and new not in text:
                raise RuntimeError("Unexpected Debian GI scanner wrapper layout")
            scanner.write_text(text.replace(old, new))


def write_env(base: Path) -> None:
    root = base / "root"
    bindir = base / "bin"
    bindir.mkdir(exist_ok=True)
    wrapper = bindir / "pkg-config"
    wrapper.write_text('#!/bin/sh\nexec /usr/bin/pkg-config "$@"\n')
    wrapper.chmod(0o755)
    python_dir = base / "python"
    python_dir.mkdir(exist_ok=True)
    # PYTHONPATH alone does not process distutils-precedence.pth. Python 3.13
    # needs the setuptools distutils shim for Debian's g-ir-scanner.
    (python_dir / "sitecustomize.py").write_text(
        "import os\nimport site\n"
        "site.addsitedir(os.environ['GIMP_DEPS_ROOT'] + '/usr/lib/python3/dist-packages')\n"
    )
    qroot = shlex.quote(str(root))
    qbin = shlex.quote(str(bindir))
    (base / "env.sh").write_text(
        "# Source this file; no system installation is performed.\n"
        f"export GIMP_DEPS_ROOT={qroot}\n"
        f'export PATH={qbin}:/usr/bin:$GIMP_DEPS_ROOT/usr/bin:$PATH\n'
        'export PKG_CONFIG_PATH=$GIMP_DEPS_ROOT/usr/lib/x86_64-linux-gnu/pkgconfig:$GIMP_DEPS_ROOT/usr/share/pkgconfig\n'
        'export LD_LIBRARY_PATH=$GIMP_DEPS_ROOT/usr/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}\n'
        'export LIBRARY_PATH=$GIMP_DEPS_ROOT/usr/lib/x86_64-linux-gnu${LIBRARY_PATH:+:$LIBRARY_PATH}\n'
        'export CPATH=$GIMP_DEPS_ROOT/usr/include:$GIMP_DEPS_ROOT/usr/include/x86_64-linux-gnu${CPATH:+:$CPATH}\n'
        f'export PYTHONPATH={shlex.quote(str(python_dir))}:$GIMP_DEPS_ROOT/usr/lib/python3/dist-packages${{PYTHONPATH:+:$PYTHONPATH}}\n'
        'export GI_TYPELIB_PATH=$GIMP_DEPS_ROOT/usr/lib/x86_64-linux-gnu/girepository-1.0${GI_TYPELIB_PATH:+:$GI_TYPELIB_PATH}\n'
        'export XDG_DATA_DIRS=$GIMP_DEPS_ROOT/usr/share:${XDG_DATA_DIRS:-/usr/share}\n'
        'export ACLOCAL_PATH=$GIMP_DEPS_ROOT/usr/share/aclocal${ACLOCAL_PATH:+:$ACLOCAL_PATH}\n'
        'export GETTEXTDATADIR=$GIMP_DEPS_ROOT/usr/share/gettext-0.23.1\n'
        'export GETTEXTDATADIRS=$GIMP_DEPS_ROOT/usr/share${GETTEXTDATADIRS:+:$GETTEXTDATADIRS}\n'
    )


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--directory", type=Path,
                        default=REPO.parent / ".deps-debian13")
    parser.add_argument("--lock", type=Path, default=DEFAULT_LOCK)
    parser.add_argument("--jobs", type=int, default=6)
    parser.add_argument("--offline", action="store_true",
                        help="Require already-downloaded, checksum-matching archives")
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error("--jobs must be positive")
    lock = json.loads(args.lock.read_text())
    if lock["format"] != 1 or lock["architecture"] != "amd64":
        parser.error("Unsupported dependency lock format or architecture")
    host = dict(line.split("=", 1) for line in Path("/etc/os-release").read_text().splitlines()
                if "=" in line and not line.startswith("#"))
    if (host.get("ID", "").strip('"') != "debian" or
            host.get("VERSION_ID", "").strip('"').split(".")[0] != "13" or
            os.uname().machine != "x86_64"):
        parser.error("This locked runtime is supported only on amd64 Debian 13")
    base = args.directory.resolve()
    apt = base / "apt"
    archives = apt / "archives"
    root = base / "root"
    for subdir in (apt / "lists/partial", archives / "partial",
                   apt / "empty", apt / "logs", root):
        subdir.mkdir(parents=True, exist_ok=True)
    (apt / "status").touch()
    (apt / "empty.conf").touch()
    snapshot = lock["snapshot"]
    distro = lock["distribution"]
    (apt / "sources.list").write_text("\n".join(
        "deb [signed-by=/usr/share/keyrings/debian-archive-keyring.gpg "
        "check-valid-until=no] https://snapshot.debian.org/archive/"
        f"{archive}/{snapshot}/ {suite} main"
        for archive, suite in (("debian", distro),
                               ("debian", distro + "-updates"),
                               ("debian-security", distro + "-security"))
    ) + "\n")
    options = {
        "Dir::Etc::parts": apt / "empty",
        "Dir::Etc::main": apt / "empty.conf",
        "Dir::Etc::sourcelist": apt / "sources.list",
        "Dir::Etc::sourceparts": apt / "empty",
        "Dir::State::lists": apt / "lists",
        "Dir::State::status": apt / "status",
        "Dir::Cache::archives": archives,
        "Dir::Cache::pkgcache": apt / "pkgcache.bin",
        "Dir::Cache::srcpkgcache": apt / "srcpkgcache.bin",
        "APT::Sandbox::User": getpass.getuser(),
        "Acquire::Languages": "none",
        "Acquire::Retries": "0",
        "Acquire::https::Timeout": "60",
    }
    config = apt / "apt.conf"
    config.write_text("".join(f'{key} "{value}";\n'
                              for key, value in options.items()))
    env = dict(os.environ, APT_CONFIG=str(config))
    command = ["/usr/bin/apt-get"]
    proxy = env.get("HTTPS_PROXY") or env.get("https_proxy")
    if proxy:
        command += ["-o", "Acquire::https::Proxy=" + proxy]
    needed = [p for p in lock["packages"] if not (archives / p["filename"]).is_file()]
    if needed and args.offline:
        raise RuntimeError(f"{len(needed)} locked archives missing in offline mode")
    if needed:
        run(command + ["update"], env, apt / "logs/update.log")

        def download(batch: tuple[int, list[dict[str, str]]]) -> None:
            index, packages = batch
            argv = command + ["download"] + [
                p["package"] + "=" + p["version"] for p in packages]
            with (apt / f"logs/download-{index:03d}.log").open("w") as output:
                subprocess.run(argv, cwd=archives, env=env, stdout=output,
                               stderr=subprocess.STDOUT, check=True)

        batches = [(n // 20, needed[n:n + 20]) for n in range(0, len(needed), 20)]
        with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
            list(pool.map(download, batches))
    # Validate every archive before any extraction. A mismatch is a hard failure;
    # the helper does not disable APT signature checks or retry another source.
    for package in lock["packages"]:
        archive = archives / package["filename"]
        if hashlib.sha256(archive.read_bytes()).hexdigest() != package["sha256"]:
            raise RuntimeError(f"Checksum mismatch: {archive}")
    for package in lock["packages"]:
        subprocess.run(["dpkg-deb", "-x", str(archives / package["filename"]),
                        str(root)], check=True)
    relocate(root)
    write_env(base)
    print(f"Verified and extracted {len(lock['packages'])} packages")
    print(f"Source {base / 'env.sh'} before configuring or compiling")


if __name__ == "__main__":
    main()
