from collections.abc import Callable
from typing import Protocol, TypeVar

from anyio import to_thread

T = TypeVar("T")


class NativeExecutor(Protocol):
    async def run(self, function: Callable[[], T]) -> T: ...


class InlineMockExecutor:
    """Run the non-blocking MockWrsGateway without creating worker threads."""

    async def run(self, function: Callable[[], T]) -> T:
        return function()


class ThreadedNativeExecutor:
    """Execute synchronous native calls outside the FastAPI event loop."""

    async def run(self, function: Callable[[], T]) -> T:
        return await to_thread.run_sync(function)
