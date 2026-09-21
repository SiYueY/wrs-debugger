import asyncio
from collections.abc import Awaitable, Callable
from typing import Protocol, TypeVar

from wrs_debugger.gateway.base import WrsGateway
from wrs_debugger.models.connection import (
    ReceiverConnection,
    ReceiverSettings,
    TransmitterConnection,
)
from wrs_debugger.models.event import EventName
from wrs_debugger.services.native_executor import NativeExecutor

Publish = Callable[[EventName, dict[str, object]], Awaitable[None]]
T = TypeVar("T")


class StateMirror(Protocol):
    def record_transmitter_connection(self, connection: TransmitterConnection) -> None: ...
    def record_receiver_connection(self, connection: ReceiverConnection) -> None: ...
    def record_receiver_settings(self, settings: ReceiverSettings) -> None: ...


class Runtime:
    def __init__(
        self,
        gateway: WrsGateway,
        publish: Publish,
        transmitter_connection: TransmitterConnection,
        receiver_connection: ReceiverConnection,
        executor: NativeExecutor,
        state_mirror: StateMirror | None = None,
    ) -> None:
        self.gateway = gateway
        self.publish = publish
        self.transmitter_connection = transmitter_connection
        self.receiver_connection = receiver_connection
        self.executor = executor
        self.state_mirror = state_mirror
        self.transmitter_lock = asyncio.Lock()
        self.receiver_lock = asyncio.Lock()
        self.operation_active: Callable[[], bool] = lambda: False

    async def call(self, function: Callable[[], T]) -> T:
        """Run all gateway calls through the one configured native executor."""
        return await self.executor.run(function)

    def set_transmitter_connection(self, connection: TransmitterConnection) -> None:
        self.transmitter_connection = connection
        if self.state_mirror is not None:
            self.state_mirror.record_transmitter_connection(connection)

    def set_receiver_connection(self, connection: ReceiverConnection) -> None:
        self.receiver_connection = connection
        if self.state_mirror is not None:
            self.state_mirror.record_receiver_connection(connection)

    def record_receiver_settings(self, settings: ReceiverSettings) -> None:
        if self.state_mirror is not None:
            self.state_mirror.record_receiver_settings(settings)

    def require_no_cross_device_operation(self) -> None:
        if self.operation_active():
            from wrs_debugger.errors import ApplicationError

            raise ApplicationError(
                "OPERATION_CONFLICT",
                409,
                "Operation conflict",
                "A cross-device operation currently owns the device locks.",
            )
