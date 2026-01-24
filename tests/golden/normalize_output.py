#!/usr/bin/env python3
from __future__ import annotations

import re
import sys


ADDR_RE = re.compile(r"[0-9A-Fa-f]{16}")
ANSI_RE = re.compile(r"\x1b\[[0-9;]*[A-Za-z]")
PROGRESS_RE = re.compile(r"^\.\.\. ")


def normalize(text: str) -> str:
    lines = []
    for line in text.splitlines():
        line = ANSI_RE.sub("", line)
        if PROGRESS_RE.match(line):
            continue
        if line.startswith("Collecting "):
            continue
        if line.startswith("Found <type "):
            continue
        if line.startswith("Warning: found <type "):
            continue
        if line.startswith("Loaded "):
            continue
        line = ADDR_RE.sub("ADDR", line)
        lines.append(line)
    return "\n".join(lines) + ("\n" if text.endswith("\n") else "")


def main() -> int:
    data = sys.stdin.read()
    sys.stdout.write(normalize(data))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
