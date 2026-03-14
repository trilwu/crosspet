#!/usr/bin/env python3
"""
Backward-compatible wrapper for ck-help tests.
Canonical suite: .claude/scripts/test_ck_help.py
"""

import runpy
from pathlib import Path


if __name__ == "__main__":
    runpy.run_path(
        str(Path(__file__).resolve().with_name("test_ck_help.py")),
        run_name="__main__",
    )
