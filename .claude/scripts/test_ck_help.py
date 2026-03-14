#!/usr/bin/env python3
"""
Smoke/integration tests for canonical ck-help script.
Run: python3 .claude/scripts/test_ck_help.py
"""

import subprocess
import sys
from pathlib import Path
from typing import List, Optional, Tuple

SCRIPT_PATH = (
    Path(__file__).resolve().parent.parent
    / "skills"
    / "ck-help"
    / "scripts"
    / "ck-help.py"
)

passed = 0
failed = 0
failures = []


def run_ck_help(args: str) -> Tuple[str, int]:
    cmd = [sys.executable, str(SCRIPT_PATH)] + (args.split() if args else [])
    result = subprocess.run(cmd, capture_output=True, text=True)
    return result.stdout + result.stderr, result.returncode


def count_output_markers(output: str) -> int:
    return len([line for line in output.splitlines() if line.startswith("@CK_OUTPUT_TYPE:")])


def test(
    name: str,
    args: str,
    expect_contains: Optional[List[str]] = None,
    expect_type: Optional[str] = None,
    expect_marker_count: Optional[int] = 1,
):
    global passed, failed, failures
    output, code = run_ck_help(args)
    errors = []

    if code != 0:
        errors.append(f"Exit code {code} != 0")

    if expect_type:
        marker = f"@CK_OUTPUT_TYPE:{expect_type}"
        if marker not in output:
            errors.append(f"Missing marker: {marker}")

    if expect_marker_count is not None:
        marker_count = count_output_markers(output)
        if marker_count != expect_marker_count:
            errors.append(f"Expected {expect_marker_count} output marker(s), got {marker_count}")

    if expect_contains:
        lower_output = output.lower()
        for text in expect_contains:
            if text.lower() not in lower_output:
                errors.append(f"Missing text: {text}")

    if errors:
        failed += 1
        failures.append((name, args, errors, output[:500]))
        print(f"❌ {name}")
        for err in errors:
            print(f"   - {err}")
    else:
        passed += 1
        print(f"✅ {name}")


def main():
    print("=" * 60)
    print("ck-help canonical test suite")
    print("=" * 60)

    test(
        "overview renders",
        "",
        expect_type="category-guide",
        expect_contains=["claudekit skills", "quick start", "/code-review"],
    )

    test(
        "category query routes correctly",
        "fix",
        expect_type="category-guide",
        expect_contains=["fixing issues", "/fix --parallel"],
    )

    test(
        "multi-word task routes to recommendation",
        "test my login",
        expect_type="task-recommendations",
        expect_contains=["recommended for", "/test"],
    )

    test(
        "subcommand space syntax resolves to command details",
        "plan archive",
        expect_type="command-details",
        expect_contains=["/plan archive", "usage"],
    )

    test(
        "legacy colon syntax alias resolves to command details",
        "plan:validate",
        expect_type="command-details",
        expect_contains=["/plan validate", "usage"],
    )

    test(
        "unknown skill query falls back to search",
        "unknown:thing",
        expect_type="search-results",
        expect_contains=["no skills found"],
    )

    print("=" * 60)
    print(f"RESULT: {passed} passed, {failed} failed")
    print("=" * 60)

    if failed:
        print("Failures:")
        for name, args, errors, preview in failures:
            print(f"- {name} ({args})")
            for err in errors:
                print(f"  - {err}")
            print(f"  preview: {preview!r}")
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
