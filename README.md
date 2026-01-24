# python-memtools

python-memtools is a memory analyzer for Python programs running on Linux, which provides deeper insight into what the target process is doing. This can help debug situations like deadlocks (both thread-based and coroutine-based) and memory leaks.

Importantly, python-memtools does not require the target process to have done something in the past (e.g. call a function that enables debugging or sets a signal handler), nor does it interfere with the process in any way other than pausing it while a memory snapshot is taken. This means that long-running jobs that only exhibit problematic behavior after several hours or days can be safely analyzed without killing or restarting them.

python-memtools is designed to debug Python 3.10 targets. Experimental build-time support for Python 3.14 is available, with some commands (like stack and thread enumeration) still unsupported.

## Building

To build python-memtools:
1. Ensure your C++ compiler has full C++23 support. GCC 13+ should suffice, as well as recent Clang versions.
2. Install cmake and readline (`sudo apt-get install readline-dev cmake`).
3. Build and install [phosg](https://github.com/fuzziqersoftware/phosg).
4. Run `cmake . && make`.

To build against Python 3.14 layout support:

    cmake -DPYMEMTOOLS_PYTHON_VERSION=314 . && make

## Debugging with python-memtools

To generate a snapshot of a Python process, run `sudo ./python-memtools --dump --pid=<PID> --path=memdump`. This will create the memdump directory and write the process’ memory contents there.

If you're in an environment where saving a memory dump to disk is infeasible (for example, in a Kubernetes pod with very limited disk space), you can use the included dump_memory.py script to make a memory dump and stream it back over an SSH connection. See the docstring in dump_memory.py for details.

Once you have a memory snapshot, you can analyze it by running `./python-memtools --path=memdump`. This will perform basic analysis, and you'll then get an analysis shell. From here, you can use the various commands to inspect the contents of the snapshot. Run `help` in the shell to see all of the available commands, and all of the options - there are more than what's listed below.

Some of the more commonly useful shell commands are:
* `repr <ADDRESS>`: Shows the contents of the object at `<ADDRESS>`. This can show the keys and values in a dict, items in a list, set, or tuple, local variables in a stack frame object, etc. When an object is found by one of the below commands, the address of that object (which can be used with `repr`) is the 16-digit hex number following the `@` after the object. This command has many options; run `help` in the shell to see what they are.
* `count-by-type`: Counts the number of objects of each type. If you see a surprisingly large number of objects of some type, that could indicate a memory leak.
* `aggregate-strings [--bytes]`: Finds all str or bytes objects and produces a histogram of their lengths. This can also be used to find all str or bytes objects whose lengths are in a specified range.
* `async-task-graph`: Finds all asyncio tasks and shows what they're waiting on, organized into a list of trees. If you ever see `<!seen>` in the output here, that indicates a deadlocked cycle of tasks awaiting each other!
* `find-all-stacks`: Finds all execution frames and organizes them into stacktraces. This is similar to what `py-spy dump` does.
* `find-all-objects --type-name=<NAME>`: Finds all objects of the specified type. Generally this is most useful for the `frame` type; if you see a lot of suspended frames in the httpx library, for example, that probably means your program is waiting on many HTTP responses from some remote service. This is also useful to find intermediate coroutines (as distinct from asyncio Tasks - there is usually not a 1:1 mapping of Tasks to coroutines).
* `find-module <NAME>`: Finds a module object. This is useful if you want to see the values of module-level global variables. If you want to get the list of all loaded modules, use `find-module sys` and look at the `modules` dict within it. (You can then use `repr` to see the contents of a specific module from that dict.)

For more advanced debugging, you can inspect raw memory with these commands:
* `regions`: Shows the list of all memory regions.
* `context <ADDRESS> [--size=<SIZE>]`: Shows `<SIZE>` bytes (default 0x100) of memory before and after `<ADDRESS>`.
* `find <HEX-DATA>` or `find "<STRING>"`: Searches for raw data or a string.

## Example scenarios

Here are some examples of issues that python-memtools can diagnose.

### Async deadlocks

The file examples/async-deadlock.py contains a simple program which results in a deadlock between async tasks. This program is a heavily-simplified version of a real issue we once encountered in one of our systems.

When you run the program, it hangs forever. Tools like py-spy will show that the process is idle in its event loop, but that doesn't explain why it's stalled:

    Thread 1822832 (idle): "MainThread"
        select (selectors.py:469)
        _run_once (asyncio/base_events.py:1871)
        run_forever (asyncio/base_events.py:603)
        run_until_complete (asyncio/base_events.py:636)
        run (asyncio/runners.py:44)
        <module> (async-stall.py:55)

To investigate further, we need to see what's in memory. First, we make a memory snapshot:

    $ sudo ./python-memtools --dump --pid=1822832 --path=stall-dump
    <...>
    28.18 MB in 76 ranges

Then, we use the `async-task-graph` command to see what the async tasks are:

    $ ./python-memtools --path=stall-dump --command=async-task-graph
    Loaded 28.16 MB in 75 regions
    Base type object not present in analysis data; looking for it
    Base type candidate found at 00005CA3F8F849A0

    No type objects are present in analysis data; looking for them
    <...>

    Looking for objects of types 00007ADDBF2FF280 (Task), 00007ADDBF2FF420 (Future), and 00005CA42B8A4750 (GatheringFuture)
    <...>
    <async task pending coro=<coroutine 'main' suspended @ 'examples/async-stall.py':50>@00007ADDBEB8DFC0>@00007ADDBEB44860
      <async task pending coro=<coroutine 'sum_all.<locals>.sum_children' suspended @ 'examples/async-stall.py':21>@00007ADDBEB8E1F0>@00007ADDBEB44E10
        <async _GatheringFuture pending>@00007ADDBE5090C0
          <async task pending coro=<coroutine 'sum_all.<locals>.sum_children' suspended @ 'examples/async-stall.py':21>@00007ADDBEB8E420>@00007ADDBEB44EE0
            <async _GatheringFuture pending>@00007ADDBE509220
              <async task pending coro=<coroutine 'sum_all.<locals>.sum_children' suspended @ 'examples/async-stall.py':21>@00007ADDBEB8E490>@00007ADDBEB44FB0
                <async _GatheringFuture pending>@00007ADDBE509170
                  <!seen>@00007ADDBEB44E10

The hexadecimal numbers after each object are the objects' memory addresses in the original Python process' address space. These uniquely identify each object (and are equal to what Python's `id()` function would return for that object). An object's address can be used with the `repr` command to get more information about the object.

The presence of `<!seen>` in the output means there's a cycle in the graph of awaiters which will never resolve. The address immediately after `<!seen>` shows the last edge of the cycle - that is, that the last asyncio.gather (`00007ADDBE509170`) is waiting for the first sum_children task (`00007ADDBEB44E10`), which itself is indirectly waiting on that asyncio.gather. To fix this, we have to implement some kind of cycle detection in the recursive algorithm in that script.

### Memory leaks

The file examples/client-memory-leak.py contains a fairly simple asyncio-based client and server. After continuous usage for a while, the client consumes more and more memory, so we suspect there may be a memory leak somewhere. Here's how we can use python-memtools to find it.

First, we make a memory smapshot of a process that's in the "bad" state (using more memory than expected).

    $ sudo ./python-memtools --dump --pid=1816596 --path=example-dump
    <...>
    897.89 MB in 85 ranges

Nothing after this point interacts with the running process; we can kill it, restart it, or even copy the memory snapshot to another box and analyze it there.

To analyze the snapshot, we then we load the memory snapshot in python-memtools:

    $ ./python-memtools --path=example-dump
    Loaded 897.88 MB in 84 regions

When investigating a memory leak, often it will be obvious if there are many more objects of a certain type than expected. So we use `count-by-type` to get a sorted list of objects:

    example-dump> count-by-type
    <...>
    (1004 objects) _asyncio.Task @ 0000768849297280
    (1013 objects) coroutine @ 0000574B59418CC0
    (2814 objects) dict @ 0000574B5941E5C0
    (3740 objects) function @ 0000574B59417D20
    (3997 objects) code @ 0000574B5941A340
    (13492 objects) tuple @ 0000574B5941BBA0
    (23328 objects) str @ 0000574B5941B7C0
    (100007 objects) _asyncio.Future @ 0000768849297420
    (102422 objects) int @ 0000574B5941F320
    (107085 objects) bytes @ 0000574B5941F180

100,000 asyncio.Future objects is a bit unexpected! We can use `find-all-objects` to see what they are (we use `--max-string-length=16` just to make the output easier to read):

    example-dump> find-all-objects --type-name=_asyncio.Future --max-string-length=16
    <async future
      finished
      loop=<_UnixSelectorEventLoop>@00007688481F4070
      result=b'\n\x9EK\xACk\x18\x9Ae\x86\xFA\xE2u\x95\xA7q\xA0'...<0x1FF0 more bytes>
    >@0000768846006090
    <...> (many more like this)

Since many of the results looked similar, we can choose any one of them and find out what objects refer to it (we use `--max-recursion-depth=0` for speed, so it won't show the contents of lists/dicts/etc.):

    example-dump> find-references 0000768846006090 --max-recursion-depth=0
    <dict !recursion_depth len=100000>@0000768848018AC0
    1 objects found

This dict has 100,000 items in it, which definitely looks like the root of the problem. Similarly to the above, we can find out where this dict is referenced from:

    example-dump> find-references 0000768848018AC0 --max-recursion-depth=1
    <Client {
      'host': 'localhost',
      'next_request_id': 100001,
      'port': 60111,
      'read_responses_task': <async task !recursion_depth>@000076884813CEE0,
      'reader': <StreamReader>@00007688481F6530,
      'response_futures': <dict !recursion_depth len=100000>,
      'writer': <StreamWriter>@00007688481F6CE0,
    }>@00007688481F65F0

So, this huge dictionary is the `response_futures` attribute on the `Client` class. We can then look at the code to figure out why we aren't deleting from `response_futures`, and indeed, we forgot to delete from `response_futures` in `Client.send_request` after the response is received.

## How it works

python-memtools works by making a snapshot of the process' memory space, then searches through it using some strong heuristics to find Python objects. This is done by first finding the base type object, which has a distinctive memory signature - its type pointer points to itself (which shouldn't be the case for any other object), its name pointer points to the string "type", and it has many other pointer fields which must either be null or point to a valid address. Once python-memtools has found the base type object, it searches for other type objects by finding all valid PyTypeObject instances whose type pointer points to the base type object.

Once this index of type objects is built, most other functions are implemented as simple memory scans that look for objects whose type pointers match one of the type pointers from the index, followed by some basic sorts, filters, or graph algorithms. Some very common types (int, str, list, dict, etc.) are implemented in python-memtools, so it can understand their contents and format them in a way that looks more like Python syntax.

## Future work

Things we'd like to do in the future:
* Add support for Python versions after 3.10
* Add support for more standard-library types (e.g. collections.deque)
* Add support for core dump files (probably can use resource_dasm's ELF loader to help with this)

## Contributing

python-memtools is a work in progress. We've found it very useful so far to debug issues that other tools couldn't solve, but we're sure there are ways in which it could be even more useful. Feel free to submit GitHub issues or PRs if you have ideas on how to improve it.

## Tests

Tests require Linux and use a helper that dumps a child process via `process_vm_readv`.
See `docs/testing-strategy.md` for the full breakdown and rationale.

Run everything:

    python3 tests/run_tests.py --tier all --version both

Run a specific tier/version:

    python3 tests/run_tests.py --tier 3 --version 314

Record golden outputs:

    python3 tests/run_tests.py --tier 2 --version 314 --golden-mode record
