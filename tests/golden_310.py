#!/usr/bin/env python3
from __future__ import annotations

import os
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from test_utils import find_python_310


def main() -> int:
    python_bin = find_python_310()
    memtools_bin = os.environ.get("MEMTOOLS_BIN", "./build/python-memtools")
    mode = os.environ.get("MODE", "compare")
    subprocess.run(
        [
            sys.executable,
            "tests/golden/run_golden.py",
            "310",
            python_bin,
            memtools_bin,
            "--mode",
            mode,
        ],
        check=True,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
