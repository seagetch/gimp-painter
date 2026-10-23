# Path miter damage (25.009/path-damage)

Native PATH/OUTLINE drawing retains miter joins, so geometric path extents plus
half-stroke padding are insufficient at sharp corners. Measure their stroke
extents instead, without changing drawing or DEFAULT/filled behavior.

Evidence and limits: `migration/tests/navigation-path-damage/README.md`.
Validation: `check-path.py`, `test-path-evidence.py`, and immutable
`replay-corner-checkpoint.py` in that directory. The original25.009196-hunk
acceptance is not closed by this repair.
