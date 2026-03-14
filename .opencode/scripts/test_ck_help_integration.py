#!/usr/bin/env python3
"""Integration tests for canonical ck-help query routing."""

import subprocess
import sys
from pathlib import Path

SCRIPT = (
    Path(__file__).resolve().parent.parent
    / "skills"
    / "ck-help"
    / "scripts"
    / "ck-help.py"
)


def run_ck_help(query: str) -> str:
    result = subprocess.run(
        [sys.executable, str(SCRIPT)] + query.split(),
        capture_output=True,
        text=True,
    )
    return result.stdout


def marker(output: str) -> str:
    for line in output.splitlines():
        if line.startswith("@CK_OUTPUT_TYPE:"):
            return line.replace("@CK_OUTPUT_TYPE:", "", 1).strip()
    return ""


def assert_route(query: str, expected_marker: str, expected_snippets: list[str]) -> bool:
    output = run_ck_help(query)
    ok = marker(output) == expected_marker
    for snippet in expected_snippets:
        ok = ok and (snippet.lower() in output.lower())
    return ok


TESTS = [
    ("fix login bug", "task-recommendations", ["/fix"]),
    ("docs update", "command-details", ["/docs update"]),
    ("plan:validate", "command-details", ["/plan validate"]),
    ("watzup", "category-guide", ["wrap-up"]),
]


def main() -> int:
    passed = 0
    failed = 0

    print("=" * 60)
    print("ck-help integration tests")
    print("=" * 60)

    for query, expected_type, snippets in TESTS:
        ok = assert_route(query, expected_type, snippets)
        print(f"{'✅' if ok else '❌'} {query!r} -> {expected_type}")
        if ok:
            passed += 1
        else:
            failed += 1

    print("=" * 60)
    print(f"RESULT: {passed} passed, {failed} failed")
    print("=" * 60)
    return 0 if failed == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
