#!/usr/bin/env python3
from __future__ import annotations

import argparse
import shutil
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from test_utils import cleanup_tempdir, dump_child, read_meta, run_memtools


def normalize_output(text: str) -> str:
    import re

    addr_re = re.compile(r"[0-9A-Fa-f]{16}")
    ansi_re = re.compile(r"\x1b\[[0-9;]*[A-Za-z]")
    out_lines = []
    for line in text.splitlines():
        line = ansi_re.sub("", line)
        if line.startswith("..."):
            continue
        if line.startswith("Collecting "):
            continue
        if line.startswith("Found <type "):
            continue
        if line.startswith("Warning: found <type "):
            continue
        if line.startswith("Loaded "):
            continue
        line = addr_re.sub("ADDR", line)
        out_lines.append(line)
    return "\n".join(out_lines) + ("\n" if text.endswith("\n") else "")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("version")
    parser.add_argument("python_bin")
    parser.add_argument("memtools_bin")
    parser.add_argument("--mode", default="compare")
    args = parser.parse_args()

    root_dir = Path(__file__).resolve().parents[2]
    tmpdir = Path(tempfile.mkdtemp())
    out_dir = root_dir / "tests" / "golden" / "output" / args.version
    gold_dir = root_dir / "tests" / "golden" / "expected" / args.version
    raw_dir = tmpdir / "raw"
    raw_dir.mkdir(parents=True, exist_ok=True)
    out_dir.mkdir(parents=True, exist_ok=True)
    gold_dir.mkdir(parents=True, exist_ok=True)

    try:
        memdump_dir = tmpdir / "memdump"
        meta_file = tmpdir / "meta.json"
        dump_child(
            python_bin=args.python_bin,
            script=root_dir / "tests" / "fixture_objects.py",
            path=memdump_dir,
            meta=meta_file,
        )

        def run_cmd(name: str, cmd: str) -> None:
            raw_text = run_memtools(args.memtools_bin, memdump_dir, cmd)
            raw_file = raw_dir / f"{name}.txt"
            raw_file.write_text(raw_text, encoding="utf-8")
            norm = normalize_output(raw_text)
            (out_dir / f"{name}.txt").write_text(norm, encoding="utf-8")

        for f in out_dir.glob("*.txt"):
            f.unlink()

        run_cmd("repr-dict", f"repr {read_meta(meta_file, 'dict')}")
        run_cmd("repr-list", f"repr {read_meta(meta_file, 'list')}")
        run_cmd("repr-tuple", f"repr {read_meta(meta_file, 'tuple')}")
        run_cmd("repr-bytes", f"repr {read_meta(meta_file, 'bytes')}")
        run_cmd("repr-str", f"repr {read_meta(meta_file, 'str')}")
        run_cmd("repr-int", f"repr {read_meta(meta_file, 'int')}")
        run_cmd("repr-bool", f"repr {read_meta(meta_file, 'bool')}")
        run_cmd("repr-float", f"repr {read_meta(meta_file, 'float')}")

        if args.mode == "record":
            for f in gold_dir.glob("*.txt"):
                f.unlink()
            for f in out_dir.glob("*.txt"):
                shutil.copy2(f, gold_dir / f.name)
            print(f"recorded golden outputs in {gold_dir}")
            return 0

        import difflib

        diffs = []
        for out_file in sorted(out_dir.glob("*.txt")):
            gold_file = gold_dir / out_file.name
            if not gold_file.exists():
                diffs.append(f"missing golden file: {gold_file}")
                continue
            out_lines = out_file.read_text(encoding="utf-8").splitlines(keepends=True)
            gold_lines = gold_file.read_text(encoding="utf-8").splitlines(keepends=True)
            if out_lines != gold_lines:
                diffs.append("".join(difflib.unified_diff(gold_lines, out_lines, fromfile=str(gold_file), tofile=str(out_file))))
        if diffs:
            sys.stderr.write("\n".join(diffs))
            return 1
        return 0
    finally:
        cleanup_tempdir(tmpdir)


if __name__ == "__main__":
    raise SystemExit(main())
