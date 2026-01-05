from __future__ import annotations

import asyncio
from dataclasses import dataclass, field


@dataclass
class Node:
    value: int
    edges: list[Node] = field(default_factory=list)


async def sum_all(root: Node) -> int:
    """Sums the values of all nodes reachable from the root, intentionally double-counting diamonds in the graph."""
    # OOPS: This algorithm doesn't detect cycles in the graph; it assumes the input is a DAG! If the graph contains a
    # cycle, this will deadlock.

    tasks: dict[int, asyncio.Task[int]] = {}

    async def sum_children(node: Node) -> int:
        return node.value + sum(await asyncio.gather(*(task_for_node(z) for z in node.edges)))

    def task_for_node(node: Node) -> asyncio.Task[int]:
        try:
            return tasks[id(node)]
        except KeyError:
            task = asyncio.create_task(sum_children(node))
            tasks[id(node)] = task
            return task

    return await task_for_node(root)


async def main() -> None:
    trivial_graph = Node(value=6, edges=[Node(value=7)])
    assert (await sum_all(trivial_graph)) == 13
    print("Trivial graph OK")

    diamond_end_node = Node(value=4)
    diamond_graph = Node(
        value=1, edges=[Node(value=2, edges=[diamond_end_node]), Node(value=3, edges=[diamond_end_node])]
    )
    assert (await sum_all(diamond_graph)) == 14
    print("Diamond graph OK")

    cycle_node_3 = Node(value=1)
    cycle_node_2 = Node(value=2, edges=[cycle_node_3])
    cycle_node_1 = Node(value=3, edges=[cycle_node_2])
    cycle_node_3.edges = [cycle_node_1]
    assert (await sum_all(cycle_node_1)) == 6
    print("Cycle graph OK")


if __name__ == "__main__":
    asyncio.run(main())
