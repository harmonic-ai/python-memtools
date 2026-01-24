#!/usr/bin/env python3
import os
import time


def main() -> None:
    # Keep some objects alive so memtools has something to find.
    data = {
        "numbers": list(range(10000)),
        "message": "hello from python-memtools",
        "nested": [{"i": i, "s": str(i)} for i in range(1000)],
    }
    print(f"PID: {os.getpid()}")
    print(f"Data keys: {list(data.keys())}")

    # Sleep forever so we can attach to this process.
    while True:
        time.sleep(1)


if __name__ == "__main__":
    main()
