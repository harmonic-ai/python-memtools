#!/usr/bin/env python3
from __future__ import annotations

import argparse
import ctypes
import ctypes.util
import json
import os
import shutil
import signal
import subprocess
import sys
import tempfile
import time
from pathlib import Path
from typing import Iterable, Sequence


DEFAULT_TIERS = ["0", "1", "2", "3", "4", "5"]
VERSION_CHOICES = ["310", "314", "both"]
TIER_CHOICES = ["0", "1", "2", "3", "4", "5", "all"]


FIXTURE_OBJECTS = """#!/usr/bin/env python3
from __future__ import annotations

import json
import time


def main() -> None:
    def sample_fn(value: int) -> int:
        return value + 1

    def make_generator():
        yield "gen-value"

    async def make_coroutine():
        return "coro-value"

    async def make_asyncgen():
        yield "asyncgen-value"

    payload = {
        "alpha": 1,
        "beta": 2,
        "message": "hello memtools",
    }
    items = [1, 2, 3, "four"]
    pair = ("left", 42)
    bag = {"red", "green", "blue"}
    data = b"bytes-data"
    text = "text-data"
    number = 123456
    flag = True
    ratio = 3.14159
    code = sample_fn.__code__
    generator = make_generator()
    coroutine = make_coroutine()
    asyncgen = make_asyncgen()

    meta = {
        "dict": f"{id(payload):016X}",
        "list": f"{id(items):016X}",
        "tuple": f"{id(pair):016X}",
        "set": f"{id(bag):016X}",
        "bytes": f"{id(data):016X}",
        "str": f"{id(text):016X}",
        "int": f"{id(number):016X}",
        "bool": f"{id(flag):016X}",
        "float": f"{id(ratio):016X}",
        "code": f"{id(code):016X}",
        "generator": f"{id(generator):016X}",
        "coroutine": f"{id(coroutine):016X}",
        "asyncgen": f"{id(asyncgen):016X}",
    }
    print(json.dumps(meta), flush=True)

    while True:
        time.sleep(1)


if __name__ == "__main__":
    main()
"""


FIXTURE_MEMORY_LEAK = """#!/usr/bin/env python3
from __future__ import annotations

import json
import time


def main() -> None:
    leak = {f"key-{idx}": idx for idx in range(5000)}
    holder = ["leak-holder", leak]

    meta = {
        "dict": f"{id(leak):016X}",
        "list": f"{id(holder):016X}",
    }
    print(json.dumps(meta), flush=True)

    while True:
        time.sleep(1)


if __name__ == "__main__":
    main()
"""


FIXTURE_FRAMES = """#!/usr/bin/env python3
from __future__ import annotations

import time


def level_one() -> None:
    level_two()


def level_two() -> None:
    level_three()


def level_three() -> None:
    time.sleep(60)


if __name__ == "__main__":
    level_one()
"""


class IOVec(ctypes.Structure):
    _fields_ = [
        ("iov_base", ctypes.c_void_p),
        ("iov_len", ctypes.c_size_t),
    ]


def run(cmd: Sequence[str], *, check: bool = True, capture: bool = True) -> subprocess.CompletedProcess:
    kwargs = {}
    if capture:
        kwargs["stdout"] = subprocess.PIPE
        kwargs["stderr"] = subprocess.STDOUT
        kwargs["text"] = True
    return subprocess.run(cmd, check=check, **kwargs)


def ensure_executable(path: str) -> None:
    p = Path(path)
    if not p.exists() or not os.access(p, os.X_OK):
        raise RuntimeError(f"memtools binary not found or not executable: {path}")


def find_python_310() -> str:
    if os.environ.get("PYTHON_BIN"):
        return os.environ["PYTHON_BIN"]
    if shutil.which("python3.10"):
        return "python3.10"
    return "python3.10"


def find_python_314() -> str:
    if os.environ.get("PYTHON_BIN"):
        return os.environ["PYTHON_BIN"]
    if shutil.which("python3.14"):
        return "python3.14"
    user_home = Path.home()
    sudo_user = os.environ.get("SUDO_USER")
    if sudo_user:
        try:
            import pwd

            user_home = Path(pwd.getpwnam(sudo_user).pw_dir)
        except Exception:
            pass
    candidate = user_home / ".pyenv" / "versions" / "3.14.2" / "bin" / "python3.14"
    if candidate.exists():
        return str(candidate)
    return "python3.14"


def read_meta(meta_file: Path, key: str) -> str:
    with meta_file.open("r", encoding="utf-8") as f:
        meta = json.load(f)
    return meta[key]


def write_failure(label: str, output: str) -> None:
    sys.stderr.write(f"{label} failed; output:\n")
    sys.stderr.write(output)


def expect_regex(pattern: str, output: str, label: str) -> None:
    import re

    if not re.search(pattern, output):
        write_failure(label, output)
        raise RuntimeError(label)


def run_memtools(memtools_bin: str, memdump_dir: Path, command: str) -> str:
    result = run([memtools_bin, f"--path={memdump_dir}", f"--command={command}"], check=False)
    if result.returncode not in (0, 1):
        raise RuntimeError(f"memtools command crashed with status {result.returncode}")
    return result.stdout or ""


def make_tempdir() -> Path:
    return Path(tempfile.mkdtemp())


def cleanup_tempdir(tmpdir: Path) -> None:
    if os.environ.get("KEEP_TMP") == "1":
        print(f"keeping temp dir: {tmpdir}", file=sys.stderr)
        return
    shutil.rmtree(tmpdir, ignore_errors=True)


def _process_vm_readv_setup() -> ctypes.CFUNCTYPE:
    libc = ctypes.CDLL(ctypes.util.find_library("c"), use_errno=True)
    fn = libc.process_vm_readv
    fn.argtypes = [
        ctypes.c_int,
        ctypes.POINTER(IOVec),
        ctypes.c_ulong,
        ctypes.POINTER(IOVec),
        ctypes.c_ulong,
        ctypes.c_ulong,
    ]
    fn.restype = ctypes.c_ssize_t
    return fn


def _ranges_for_pid(pid: int) -> list[tuple[int, int]]:
    ranges: list[tuple[int, int]] = []
    with open(f"/proc/{pid}/maps", "rt", encoding="utf-8") as maps_f:
        for line in maps_f:
            tokens = line.split()
            if len(tokens) < 2:
                continue
            if tokens[1][0] != "r":
                continue
            if tokens[1][3] == "s":
                continue
            start_str, end_str = tokens[0].split("-")
            start = int(start_str, 16)
            end = int(end_str, 16)
            ranges.append((start, end))
    return ranges


def _dump_ranges(pid: int, out_dir: str, chunk_size: int = 1024 * 1024) -> None:
    os.makedirs(out_dir, exist_ok=True)
    readv = _process_vm_readv_setup()

    ranges = _ranges_for_pid(pid)
    total_size = sum(end - start for start, end in ranges)
    for start, end in ranges:
        filename = os.path.join(out_dir, f"mem.{start:016X}.{end:016X}.bin")
        written = 0
        with open(filename, "wb") as out_f:
            addr = start
            while addr < end:
                size = min(chunk_size, end - addr)
                buf = ctypes.create_string_buffer(size)
                local = IOVec(ctypes.cast(buf, ctypes.c_void_p), size)
                remote = IOVec(ctypes.c_void_p(addr), size)
                nread = readv(pid, ctypes.byref(local), 1, ctypes.byref(remote), 1, 0)
                if nread <= 0:
                    err = ctypes.get_errno()
                    print(
                        f"... {addr:016X}:{addr + size:016X} (failed: {os.strerror(err)})",
                        file=sys.stderr,
                    )
                    out_f.write(b"\x00" * size)
                    written += size
                    addr += size
                    continue
                out_f.write(buf.raw[:nread])
                written += nread
                if nread < size:
                    out_f.write(b"\x00" * (size - nread))
                    written += size - nread
                addr += size
        print(f"... {start:016X}:{end:016X} ({written} bytes)", file=sys.stderr)

    total_gb = total_size / (1024 * 1024 * 1024)
    print(f"{total_size:X} bytes ({total_gb:.02f} GB) in {len(ranges)} ranges", file=sys.stderr)


def _write_fixture(tmpdir: Path, name: str, text: str) -> Path:
    path = tmpdir / name
    path.write_text(text, encoding="utf-8")
    return path


def dump_child(
    *,
    python_bin: str,
    script_text: str,
    path: Path,
    script_name: str = "fixture_child.py",
    meta: Path | None = None,
    wait: float | None = None,
) -> None:
    wait_time = 1.0 if wait is None else wait
    tmpdir = path.parent
    script_path = _write_fixture(tmpdir, script_name, script_text)

    child = subprocess.Popen(
        [python_bin, str(script_path)],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )
    try:
        if meta is not None:
            meta_line = ""
            if child.stdout:
                meta_line = child.stdout.readline().strip()
            if not meta_line:
                output = child.stdout.read() if child.stdout else ""
                raise RuntimeError(f"child did not emit metadata: {output}")
            meta.write_text(meta_line + "\n", encoding="utf-8")

        time.sleep(wait_time)
        if child.poll() is not None:
            output = child.stdout.read() if child.stdout else ""
            raise RuntimeError(f"child process exited early; output:\n{output}")

        os.kill(child.pid, signal.SIGSTOP)
        try:
            _dump_ranges(child.pid, str(path))
        finally:
            os.kill(child.pid, signal.SIGCONT)
    finally:
        child.terminate()
        try:
            child.wait(timeout=3)
        except subprocess.TimeoutExpired:
            child.kill()


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


def build_matrix() -> None:
    subprocess.run(["cmake", "-S", ".", "-B", "build", "-DPYMEMTOOLS_PYTHON_VERSION=310"], check=True)
    subprocess.run(["cmake", "--build", "build"], check=True)
    subprocess.run(["cmake", "-S", ".", "-B", "build-314", "-DPYMEMTOOLS_PYTHON_VERSION=314"], check=True)
    subprocess.run(["cmake", "--build", "build-314"], check=True)


def run_smoke(python_bin: str, memtools_bin: str) -> None:
    ensure_executable(memtools_bin)
    tmpdir = make_tempdir()
    try:
        memdump_dir = tmpdir / "memdump"
        meta_file = tmpdir / "meta.json"
        dump_child(
            python_bin=python_bin,
            script_text=FIXTURE_OBJECTS,
            path=memdump_dir,
            script_name="fixture_objects.py",
            meta=meta_file,
        )

        output = run_memtools(memtools_bin, memdump_dir, "count-by-type")
        expect_regex(r" dict @ [0-9A-Fa-f]{16}", output, "count-by-type")

        output = run_memtools(memtools_bin, memdump_dir, "find-all-objects --type-name=dict --count")
        expect_regex(r"[0-9]+ objects found", output, "find-all-objects")

        import re

        type_output = run_memtools(memtools_bin, memdump_dir, "count-by-type")
        match = re.search(r" dict @ ([0-9A-Fa-f]{16})", type_output)
        if not match:
            raise RuntimeError("dict type not found in count-by-type")
        type_addr = match.group(1)

        output = run_memtools(memtools_bin, memdump_dir, f"repr {type_addr}")
        expect_regex(r"<type dict>", output, "repr dict type")

        dict_addr = read_meta(meta_file, "dict")
        list_addr = read_meta(meta_file, "list")
        tuple_addr = read_meta(meta_file, "tuple")
        set_addr = read_meta(meta_file, "set")
        bytes_addr = read_meta(meta_file, "bytes")
        str_addr = read_meta(meta_file, "str")
        int_addr = read_meta(meta_file, "int")

        output = run_memtools(memtools_bin, memdump_dir, f"repr {dict_addr}")
        expect_regex(r"alpha", output, "repr dict")

        output = run_memtools(memtools_bin, memdump_dir, f"repr {list_addr}")
        expect_regex(r"four", output, "repr list")

        output = run_memtools(memtools_bin, memdump_dir, f"repr {tuple_addr}")
        expect_regex(r"left", output, "repr tuple")

        output = run_memtools(memtools_bin, memdump_dir, f"repr {set_addr}")
        expect_regex(r"green", output, "repr set")

        output = run_memtools(memtools_bin, memdump_dir, f"repr {bytes_addr}")
        expect_regex(r"bytes-data", output, "repr bytes")

        output = run_memtools(memtools_bin, memdump_dir, f"repr {str_addr}")
        expect_regex(r"text-data", output, "repr str")

        output = run_memtools(memtools_bin, memdump_dir, f"repr {int_addr}")
        expect_regex(r"123456", output, "repr int")

        output = run_memtools(
            memtools_bin,
            memdump_dir,
            "aggregate-strings --print-smaller-than=32 --print-larger-than=0",
        )
        expect_regex(r"Found [0-9]+ objects", output, "aggregate-strings")
    finally:
        cleanup_tempdir(tmpdir)


def run_golden(version: str, python_bin: str, memtools_bin: str, mode: str) -> None:
    root_dir = Path(__file__).resolve().parents[1]
    tmpdir = Path(tempfile.mkdtemp())
    out_dir = root_dir / "tests" / "golden" / "output" / version
    gold_dir = root_dir / "tests" / "golden" / "expected" / version
    raw_dir = tmpdir / "raw"
    raw_dir.mkdir(parents=True, exist_ok=True)
    out_dir.mkdir(parents=True, exist_ok=True)
    gold_dir.mkdir(parents=True, exist_ok=True)

    try:
        memdump_dir = tmpdir / "memdump"
        meta_file = tmpdir / "meta.json"
        dump_child(
            python_bin=python_bin,
            script_text=FIXTURE_OBJECTS,
            path=memdump_dir,
            script_name="fixture_objects.py",
            meta=meta_file,
        )

        def run_cmd(name: str, cmd: str) -> None:
            raw_text = run_memtools(memtools_bin, memdump_dir, cmd)
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

        if mode == "record":
            for f in gold_dir.glob("*.txt"):
                f.unlink()
            for f in out_dir.glob("*.txt"):
                shutil.copy2(f, gold_dir / f.name)
            print(f"recorded golden outputs in {gold_dir}")
            return

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
            raise RuntimeError("golden output mismatch")
    finally:
        cleanup_tempdir(tmpdir)


def run_layout(python_bin: str, memtools_bin: str) -> None:
    tmpdir = make_tempdir()
    try:
        memdump_dir = tmpdir / "memdump"
        meta_file = tmpdir / "meta.json"
        dump_child(
            python_bin=python_bin,
            script_text=FIXTURE_OBJECTS,
            path=memdump_dir,
            script_name="fixture_objects.py",
            meta=meta_file,
        )

        run_memtools(memtools_bin, memdump_dir, "find-all-types")

        keys = [
            "dict",
            "list",
            "tuple",
            "set",
            "bytes",
            "str",
            "int",
            "bool",
            "float",
            "code",
            "generator",
            "coroutine",
            "asyncgen",
        ]
        object_addrs = ",".join(read_meta(meta_file, key) for key in keys)

        output = run_memtools(memtools_bin, memdump_dir, f"validate-layouts --object-addr={object_addrs}")
        for line in output.splitlines():
            if line.startswith("object ") or line.startswith("type ") or line.startswith("  reason ") or line.startswith("invalid objects"):
                print(line)
        import re

        if re.search(r"invalid objects: [1-9]", output):
            raise RuntimeError("layout validation failed")
    finally:
        cleanup_tempdir(tmpdir)


def run_async_graph(version: str, python_bin: str, memtools_bin: str) -> None:
    tmpdir = make_tempdir()
    try:
        memdump_dir = tmpdir / "memdump"
        dump_child(
            python_bin=python_bin,
            script_text=Path("examples/async-stall.py").read_text(encoding="utf-8"),
            path=memdump_dir,
            script_name="fixture_async_stall.py",
            wait=2.0,
        )
        output = run_memtools(memtools_bin, memdump_dir, "async-task-graph")
        if version == "314":
            if "async _GatheringFuture" not in output:
                raise RuntimeError("async-task-graph did not find gathering futures")
            if "async future" not in output:
                raise RuntimeError("async-task-graph did not find futures")
        else:
            import re

            if not re.search(r"<!seen>@[0-9A-Fa-f]{16}", output):
                raise RuntimeError("async-task-graph did not report a cycle")
    finally:
        cleanup_tempdir(tmpdir)


def run_memory_leak(python_bin: str, memtools_bin: str) -> None:
    tmpdir = make_tempdir()
    try:
        memdump_dir = tmpdir / "memdump"
        meta_file = tmpdir / "meta.json"
        dump_child(
            python_bin=python_bin,
            script_text=FIXTURE_MEMORY_LEAK,
            path=memdump_dir,
            script_name="fixture_memory_leak.py",
            meta=meta_file,
        )
        dict_addr = read_meta(meta_file, "dict")
        output = run_memtools(
            memtools_bin,
            memdump_dir,
            f"find-references {dict_addr} --max-entries=3 --max-recursion-depth=2",
        )
        if "leak-holder" not in output:
            raise RuntimeError("find-references did not locate the leak holder")
    finally:
        cleanup_tempdir(tmpdir)


def run_frames(version: str, python_bin: str, memtools_bin: str) -> None:
    tmpdir = make_tempdir()
    try:
        memdump_dir = tmpdir / "memdump"
        dump_child(
            python_bin=python_bin,
            script_text=FIXTURE_FRAMES,
            path=memdump_dir,
            script_name="fixture_frames.py",
            wait=1.0,
        )
        command = "find-all-stacks --include-runnable" if version == "310" else "find-all-stacks"
        output = run_memtools(memtools_bin, memdump_dir, command)
        if "fixture_frames.py" not in output:
            raise RuntimeError("find-all-stacks did not report fixture_frames.py")
    finally:
        cleanup_tempdir(tmpdir)


def run_robust(python_bin: str, memtools_bin: str) -> None:
    tmpdir = make_tempdir()
    try:
        memdump_dir = tmpdir / "memdump"
        meta_file = tmpdir / "meta.json"
        dump_child(
            python_bin=python_bin,
            script_text=FIXTURE_OBJECTS,
            path=memdump_dir,
            script_name="fixture_objects.py",
            meta=meta_file,
        )
        dict_addr = read_meta(meta_file, "dict")
        output = run_memtools(memtools_bin, memdump_dir, f"repr {dict_addr}")
        if "Loaded" not in output:
            raise RuntimeError("baseline command did not start properly")

        memfiles = sorted(memdump_dir.glob("mem.*.bin"))
        if not memfiles:
            raise RuntimeError("no memdump files found")
        memfile = memfiles[0]

        zero_dir = tmpdir / "memdump-zero"
        trunc_dir = tmpdir / "memdump-trunc"
        shutil.copytree(memdump_dir, zero_dir)
        shutil.copytree(memdump_dir, trunc_dir)

        with open(zero_dir / memfile.name, "r+b") as f:
            f.write(b"\x00" * 4096)

        (trunc_dir / memfile.name).write_bytes(b"")

        run_memtools(memtools_bin, zero_dir, "count-by-type")
        run_memtools(memtools_bin, trunc_dir, "count-by-type")
    finally:
        cleanup_tempdir(tmpdir)


def resolve_bins(version: str, args: argparse.Namespace) -> tuple[str, str]:
    if version == "310":
        python_bin = args.python_310 or os.environ.get("PYTHON_310")
        if not python_bin and args.python_bin and args.version != "both":
            python_bin = args.python_bin
        if not python_bin:
            python_bin = find_python_310()
        memtools_bin = args.memtools_310 or os.environ.get("MEMTOOLS_310") or "./build/python-memtools"
    elif version == "314":
        python_bin = args.python_314 or os.environ.get("PYTHON_314")
        if not python_bin and args.python_bin and args.version != "both":
            python_bin = args.python_bin
        if not python_bin:
            python_bin = find_python_314()
        memtools_bin = args.memtools_314 or os.environ.get("MEMTOOLS_314") or "./build-314/python-memtools"
    else:
        raise ValueError(f"unsupported version: {version}")
    return python_bin, memtools_bin


def run_tiers(version: str, tiers: list[str], args: argparse.Namespace) -> None:
    python_bin, memtools_bin = resolve_bins(version, args)

    if "0" in tiers:
        build_matrix()

    if "1" in tiers:
        run_smoke(python_bin, memtools_bin)

    if "2" in tiers:
        run_golden(version, python_bin, memtools_bin, args.golden_mode)

    if "3" in tiers:
        run_layout(python_bin, memtools_bin)

    if "4" in tiers:
        run_async_graph(version, python_bin, memtools_bin)
        run_memory_leak(python_bin, memtools_bin)
        run_frames(version, python_bin, memtools_bin)

    if "5" in tiers:
        run_robust(python_bin, memtools_bin)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run python-memtools test tiers.")
    parser.add_argument("--tier", default="all", choices=TIER_CHOICES)
    parser.add_argument("--version", default="both", choices=VERSION_CHOICES)
    parser.add_argument("--golden-mode", default=os.environ.get("MODE", "compare"), choices=["compare", "record"])
    parser.add_argument("--python-bin", dest="python_bin")
    parser.add_argument("--python-310", dest="python_310")
    parser.add_argument("--python-314", dest="python_314")
    parser.add_argument("--memtools-310", dest="memtools_310")
    parser.add_argument("--memtools-314", dest="memtools_314")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    tiers = DEFAULT_TIERS if args.tier == "all" else [args.tier]

    if args.version == "both":
        for version in ("310", "314"):
            run_tiers(version, tiers, args)
    else:
        run_tiers(args.version, tiers, args)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
