# Testing Strategy

This document describes a five-tier testing strategy for python-memtools, from fast smoke checks to deep layout validation and robustness testing.

## Tier 0: Build Matrix
- Build both versions: `PYMEMTOOLS_PYTHON_VERSION=310` and `PYMEMTOOLS_PYTHON_VERSION=314`.
- Treat warnings as errors (current build flags already do this).
- Ensure both binaries link correctly against phosg.
- Or run everything at once: `tests/run_tests.py --tier all --version both`.

## Tier 1: Smoke Tests
- Run `tests/run_tests.py --tier 1 --version 310` and `tests/run_tests.py --tier 1 --version 314`.
- Extend smoke validation to include a short command set:
  - `count-by-type`
  - `find-all-objects --type-name=dict --max-results=3`
  - `repr <ADDRESS>` for a known dict
  - `aggregate-strings --print-smaller-than=32 --print-larger-than=0`

## Tier 2: Golden Snapshot Tests (layout validation)
- For each version, run a deterministic Python script that creates known objects:
  - dict/list/tuple/bytes/str/int
- Dump memory and run a fixed analysis script that reps those objects.
- Save output into versioned golden files (e.g. `tests/golden/expected/310`, `tests/golden/expected/314`).
- Compare outputs against goldens:
  - `tests/run_tests.py --tier 2 --version 310`
  - `tests/run_tests.py --tier 2 --version 314`
- To refresh goldens, run with `MODE=record`.

## Tier 3: Targeted Layout Tests
- Run layout validation on a focused set of types using the analyzer itself:
  - `tests/run_tests.py --tier 3 --version 310`
  - `tests/run_tests.py --tier 3 --version 314`
- The command runs `invalid_reason` checks across all objects of these types and fails if any are invalid.
- Current list includes generators/coroutines/async generators for 3.14 layout coverage.
- Over time, expand the type list to include more moved structs:
  - `PyTypeObject` (tail fields, `tp_watched`)
  - `PyCodeObject` (localsplus, linetable, qualname)
  - `PyDictObject` and `PyDictKeysObject` (split tables, index sizes)

## Tier 4: Behavioral Tests
- Async deadlocks: `tests/run_tests.py --tier 4 --version 310` runs `async-task-graph` against
  `examples/async-stall.py` and looks for cycle markers; `tests/run_tests.py --tier 4 --version 314` verifies futures are discovered.
- Memory leak scenario: `tests/run_tests.py --tier 4 --version 310` and `tests/run_tests.py --tier 4 --version 314` validate `find-references` against
  `tests/fixture_memory_leak.py`.
- Stacks/frames: `tests/run_tests.py --tier 4 --version 310` and `tests/run_tests.py --tier 4 --version 314` validate `find-all-stacks` on a controlled call stack.

## Tier 5: Robustness and Fuzz
- `tests/run_tests.py --tier 5 --version 310` and `tests/run_tests.py --tier 5 --version 314` run a robustness pass:
  - Baseline command on a clean dump.
  - Corrupt dumps (truncate and zero a region) and ensure the analyzer exits without crashing.
