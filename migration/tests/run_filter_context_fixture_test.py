#!/usr/bin/env python3
"""Run an already-built scalar test using the verified compact oracle bundle."""
from pathlib import Path
import subprocess
import sys
import tempfile

from filter_context_fixture_bundle import materialize

if __name__ == "__main__":
    executable, archive, manifest = map(Path, sys.argv[1:4])
    with tempfile.TemporaryDirectory(prefix="painter-context-fixtures-") as directory:
        verified = materialize(archive, manifest, Path(directory))
        if verified["roots"] != ["legacy-filter-context"]:
            raise SystemExit("Unexpected Filter context oracle root")
        result = subprocess.run([str(executable), str(Path(directory) / "legacy-filter-context")] + sys.argv[4:])
    raise SystemExit(result.returncode)
