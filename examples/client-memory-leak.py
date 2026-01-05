from __future__ import annotations

import asyncio
import logging
import random
import struct
from dataclasses import dataclass
from typing import TYPE_CHECKING, Any

if TYPE_CHECKING:
    from collections.abc import Coroutine


class AsyncTaskSet:
    def __init__(self) -> None:
        self.tasks: set[asyncio.Task[Any]] = set()

    def add(self, task: asyncio.Task[Any] | Coroutine[None, None, Any]) -> None:
        if not isinstance(task, asyncio.Task):
            task = asyncio.create_task(task)
        self.tasks.add(task)
        task.add_done_callback(self._on_task_done)

    def _on_task_done(self, task: asyncio.Task[Any]) -> None:
        self.tasks.discard(task)
        try:
            task.result()
        except asyncio.CancelledError:
            pass
        except Exception as e:
            logging.exception("Exception suppressed in AsyncTaskSet task")
            self.most_recent_exception = e


@dataclass
class Message:
    # The protocol is very simple; each message consists of:
    #   uint32_t request_id
    #   uint32_t data_size
    #   uint8_t data[data_size]
    request_id: int
    data: bytes

    @classmethod
    async def read(cls, reader: asyncio.StreamReader) -> Message:
        header = await reader.readexactly(8)
        request_id, data_size = struct.unpack("<LL", header)
        data = await reader.readexactly(data_size)
        return cls(request_id=request_id, data=data)

    def write(self, writer: asyncio.StreamWriter) -> None:
        writer.write(struct.pack("<LL", self.request_id, len(self.data)))
        writer.write(self.data)


class EchoServer:
    """Basic async server that waits a random length of time for each query (up to 1 second) and then returns whatever
    the client sent."""

    def __init__(self, port: int) -> None:
        self.port: int = port

    async def _handle_message(self, writer: asyncio.StreamWriter, message: Message) -> None:
        await asyncio.sleep(random.random())
        message.write(writer)

    async def _handle_connection(self, reader: asyncio.StreamReader, writer: asyncio.StreamWriter) -> None:
        tasks = AsyncTaskSet()
        while True:
            message = await Message.read(reader)
            tasks.add(self._handle_message(writer, message))

    async def run(self):
        server = await asyncio.start_server(self._handle_connection, port=self.port)
        async with server:
            await server.serve_forever()


class Client:
    """Basic async client that sends encapsulated byte strings to a server, and receives a response for each one.
    Multiple "queries" may be in progress at any given time and the responses may be received in any order, so we use
    the request ID / response_future pattern."""

    def __init__(self, host: str, port: int) -> None:
        self.host: str = host
        self.port: int = port
        self.next_request_id: int = 1
        self.reader: asyncio.StreamReader
        self.writer: asyncio.StreamWriter
        self.response_futures: dict[int, asyncio.Future[bytes]] = {}
        self.read_responses_task: asyncio.Task[None]

    async def __aenter__(self):
        self.reader, self.writer = await asyncio.open_connection(host=self.host, port=self.port)
        self.read_responses_task = asyncio.create_task(self._read_responses())
        return self

    async def __aexit__(self, t, v, tb):
        self.read_responses_task.cancel()
        for future in self.response_futures.values():
            if not future.done():
                future.set_exception(RuntimeError("Connection was closed"))
        self.writer.close()
        await self.writer.wait_closed()

    async def _read_responses(self):
        while True:
            message = await Message.read(self.reader)
            try:
                future = self.response_futures[message.request_id]
            except KeyError:
                continue
            if not future.done():
                future.set_result(message.data)

    async def send_request(self, data: bytes) -> bytes:
        request_id = self.next_request_id
        self.next_request_id += 1

        future = asyncio.get_running_loop().create_future()
        self.response_futures[request_id] = future
        Message(request_id=request_id, data=data).write(self.writer)
        # _read_responses will resolve this future when the corresponding response is received
        return await future
        # OOPS: We forgot to delete from self.response_futures here!


async def main() -> None:
    port = 60111

    server = EchoServer(port=port)
    server_task = asyncio.create_task(server.run())
    await asyncio.sleep(0.5)  # Wait for server task to start
    try:
        async with Client(host="localhost", port=port) as client:
            num_requests_completed: int = 0
            num_tasks: int = 1000
            num_requests_per_task: int = 100
            bytes_per_request: int = 1024 * 8  # 8KB

            async def send_requests() -> None:
                nonlocal num_requests_completed
                for _ in range(num_requests_per_task):
                    request_data = random.randbytes(bytes_per_request)
                    response_data = await client.send_request(request_data)
                    assert response_data == request_data
                    num_requests_completed += 1
                    print(f"... {num_requests_completed}/{num_tasks * num_requests_per_task}")

            await asyncio.gather(*(send_requests() for _ in range(num_tasks)))
            input("Press Enter to continue...")

    finally:
        server_task.cancel()


if __name__ == "__main__":
    asyncio.run(main())
