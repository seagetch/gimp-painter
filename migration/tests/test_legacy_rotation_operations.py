"""Discover sealed original rotation evidence tests; no fresh GUI execution."""
import importlib.util
from pathlib import Path
import unittest

def load_tests(loader, tests, pattern):
    path = Path(__file__).resolve().parents[1] / "fixtures/legacy-rotation-operations/test_rotation_capture.py"
    spec = importlib.util.spec_from_file_location("legacy_rotation_evidence", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return loader.loadTestsFromModule(module)
