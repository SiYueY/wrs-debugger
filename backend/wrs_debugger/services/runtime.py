import asyncio
from collections.abc import Awaitable, Callable
from typing import Protocol, TypeVar, runtime_checkable

from wrs_debugger.errors import ApplicationError
from wrs_debugger.gateway.base import WrsGateway
from wrs_debugger.models.connection import (
    ConnectionError,
    ConnectionState,
    ReceiverConnection,
    ReceiverSettings,
    TransmitterConnection,
)
from wrs_debugger.models.event import EventName
from wrs_debugger.models.system import BackendPhase
from wrs_debugger.services.native_executor import NativeExecutor

Publish = Callable[[EventName, object], Awaitable[None]]
T = TypeVar("T")

_TRANSMITTER_DISCONNECT_CODES = frozenset(
    {"TRANSMITTER_DISCONNECTED", "TRANSMITTER_NOT_CONNECTED"}
)
_RECEIVER_DISCONNECT_CODES = frozenset({"RECEIVER_DISCONNECTED", "RECEIVER_NOT_CONNECTED"})


@runtime_checkable
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
        self.phase = BackendPhase.ready_for_control
        self.transmitter_connection = transmitter_connection
        self.receiver_connection = receiver_connection
        self.executor = executor
        self.state_mirror = state_mirror
        self.transmitter_lock = asyncio.Lock()
        self.receiver_lock = asyncio.Lock()
        self.operation_active: Callable[[], bool] = lambda: False

    async def call(self, function: Callable[[], T]) -> T:
        """Run all gateway calls through the configured native executor."""
        return await self.executor.run(function)

    async def call_transmitter(self, function: Callable[[], T]) -> T:
        try:
            return await self.call(function)
        except ApplicationError as error:
            if error.code in _TRANSMITTER_DISCONNECT_CODES:
                self.set_transmitter_connection(
                    TransmitterConnection(
                        state=ConnectionState.disconnected,
                        error=ConnectionError(code=error.code, detail=error.detail),
                    )
                )
                await self.publish(
                    EventName.transmitter_connection_changed,
                    self.transmitter_connection.model_dump(mode="json"),
                )
            raise

    async def call_receiver(self, function: Callable[[], T]) -> T:
        try:
            return await self.call(function)
        except ApplicationError as error:
            if error.code in _RECEIVER_DISCONNECT_CODES:
                self.set_receiver_connection(
                    ReceiverConnection(
                        state=ConnectionState.disconnected,
                        error=ConnectionError(code=error.code, detail=error.detail),
                    )
                )
                await self.publish(
                    EventName.receiver_connection_changed,
                    self.receiver_connection.model_dump(mode="json"),
                )
            raise

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
            raise ApplicationError(
                "OPERATION_CONFLICT",
                409,
                "Operation conflict",
                "A cross-device operation currently owns the device locks.",
            )
