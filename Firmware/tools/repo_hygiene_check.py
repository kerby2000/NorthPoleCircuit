#!/usr/bin/env python3
"""Fail on generated artifacts and stale scaffold paths that should not be committed."""

from __future__ import annotations

import sys
from pathlib import Path


BLOCKED_DIR_NAMES = {"__pycache__", "build", ".settings", ".history"}
BLOCKED_SUFFIXES = {
    ".pyc",
    ".pyo",
    ".pyd",
    ".elf",
    ".hex",
    ".bin",
    ".map",
    ".log",
    ".kicad_prl",
}
BLOCKED_PRODUCTION_SUFFIXES = {".zip", ".xls", ".xlsx", ".csv", ".ipc"}
BLOCKED_RELATIVE_DIRS = {
    Path("Firmware/src"),
    Path("Firmware/include"),
}
SKIP_DIR_NAMES = {".git"}


def repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def is_under(path: Path, parent: Path) -> bool:
    try:
        path.relative_to(parent)
        return True
    except ValueError:
        return False


def scan(root: Path) -> list[Path]:
    findings: list[Path] = []

    for blocked in BLOCKED_RELATIVE_DIRS:
        candidate = root / blocked
        if candidate.exists():
            findings.append(candidate)

    def visit(directory: Path) -> None:
        try:
            entries = list(directory.iterdir())
        except OSError as exc:
            findings.append(directory / f"<unreadable: {exc}>")
            return

        for entry in entries:
            if entry.name in SKIP_DIR_NAMES:
                continue
            if entry.is_dir():
                if entry.name in BLOCKED_DIR_NAMES or entry.name.endswith("-backups"):
                    findings.append(entry)
                    continue
                visit(entry)
            elif entry.suffix.lower() in BLOCKED_SUFFIXES:
                findings.append(entry)
            elif is_under(entry, root / "PCB" / "production") and entry.suffix.lower() in BLOCKED_PRODUCTION_SUFFIXES:
                findings.append(entry)

    visit(root)

    # Avoid duplicate reports when an exact blocked relative dir was also found by name.
    deduped: list[Path] = []
    seen: set[Path] = set()
    for finding in findings:
        resolved = finding.resolve()
        if any(is_under(resolved, existing) for existing in seen):
            continue
        if resolved not in seen:
            seen.add(resolved)
            deduped.append(finding)
    return deduped


def main() -> int:
    root = repo_root()
    findings = scan(root)
    if findings:
        print("Repo hygiene check failed. Remove these generated/stale paths before commit:")
        for finding in sorted(findings):
            print(f"  {finding.relative_to(root)}")
        return 1

    print("Repo hygiene check passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
