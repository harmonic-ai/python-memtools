"""This script makes a memory snapshot in either of the formats used by python-memtools.

This can be used to make snapshots on machines where python-memtools can't be built (for example, because it uses C++23
and the machine's compiler is too old). If python-memtools can be built on the machine, you should use python-memtools
itself to create the snapshot, since the process implemented there is much more efficient:
  python-memtools --dump --pid=<PID> --path=<PATH>

This script can also be used to make a memory dump in places where there isn't enough space on disk to save it, such as
a GKE pod with limited ephemeral storage. To do this, you need to forward stdout in binary mode through e.g. an SSH
session, then save it to disk on the client, like this (you will have to copy):
  kubectl exec POD-NAME -c DEBUG-CONTAINER -- python3 dump_memory.py --pid PID --stream > memdump.bin

"""

from __future__ import annotations

import argparse
import os
import signal
import struct
import sys


def dump_memory(pid: int, path: str | None) -> None:
    if path is not None:
        os.makedirs(path, exist_ok=True)

    os.kill(pid, signal.SIGSTOP)
    try:
        ranges: list[tuple[int, int]] = []
        total_size: int = 0
        with open(f"/proc/{pid}/maps", "rt") as maps_f:
            maps_lines = maps_f.read().splitlines()
        for line in maps_lines:
            tokens = line.split()
            if tokens[1][0] != "r":  # Skip non-readable memory
                continue
            if tokens[1][3] == "s":  # Skip shared memory regions (they won't contain Python objects)
                continue
            start_str, end_str = tokens[0].split("-")
            start = int(start_str, 16)
            end = int(end_str, 16)
            ranges.append((start, end))
            total_size += end - start

        total_gb = total_size / (1024 * 1024 * 1024)
        with open(f"/proc/{pid}/mem", "rb") as mem_f:
            for start, end in ranges:
                try:
                    mem_f.seek(start, 0)
                    data = mem_f.read(end - start)
                    if path is None:
                        sys.stdout.buffer.write(struct.pack("<QQ", start, start + len(data)))
                        sys.stdout.buffer.write(data)
                    else:
                        filename = os.path.join(path, f"mem.{start:016X}.{end:016X}.bin")
                        with open(filename, "wb") as out_f:
                            out_f.write(data)
                    print(f"... {start:016X}:{end:016X}", file=sys.stderr)
                except Exception as e:
                    print(f"... {start:016X}:{end:016X} (failed: {e!r})", file=sys.stderr)

        print(f"{total_size:X} bytes ({total_gb:.02f} GB) in {len(ranges)} ranges", file=sys.stderr)

    finally:
        os.kill(pid, signal.SIGCONT)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--pid", type=int, required=True)
    parser.add_argument("--path", type=str, default=None)
    parser.add_argument("--stream", action="store_true", default=False)
    args = parser.parse_args()

    assert args.pid is not None, "--pid is required"
    assert args.path is not None or args.stream, "Either --path or --stream is required"
    dump_memory(args.pid, args.path)

    return 0


if __name__ == "__main__":
    sys.exit(main())
