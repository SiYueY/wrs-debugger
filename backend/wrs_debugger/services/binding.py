from collections.abc import Awaitable, Callable

from wrs_debugger.errors import ApplicationError
from wrs_debugger.models.connection import ConnectionState
from wrs_debugger.models.event import EventName
from wrs_debugger.models.operation import Operation, OperationType
from wrs_debugger.operations.manager import OperationManager
from wrs_debugger.services.runtime import Runtime

ProgressUpdate = Callable[[str, float], Awaitable[None]]


class FactoryBindingService:
    def __init__(self, runtime: Runtime, operations: OperationManager) -> None:
        self.runtime = runtime
        self.operations = operations

    async def bind(self) -> Operation:
        self._require_connections()

        async def work(update: ProgressUpdate) -> None:
            # This is the fixed cross-device lock order used by all V1 operations.
            async with self.runtime.transmitter_lock:
                handle = await self.runtime.call(self.runtime.gateway.prepare_transmitter_binding)
                try:
                    await update("preparing_transmitter", 0.3)
                    async with self.runtime.receiver_lock:
                        await self.runtime.call(
                            lambda: self.runtime.gateway.prepare_receiver_binding(handle)
                        )
                        await update("binding_receiver", 0.65)
                        await self.runtime.call(lambda: self.runtime.gateway.verify_binding(handle))
                    await update("verifying_binding", 0.9)
                except BaseException:
                    await self.runtime.call(lambda: self.runtime.gateway.rollback_binding(handle))
                    raise
            receiver_info = await self.runtime.call(self.runtime.gateway.read_receiver_info)
            await self.runtime.publish(
                EventName.receiver_info_changed, receiver_info.model_dump(mode="json")
            )

        return await self.operations.start(OperationType.factory_binding, work)

    def _require_connections(self) -> None:
        if self.runtime.transmitter_connection.state != ConnectionState.connected:
            raise ApplicationError(
                "TRANSMITTER_NOT_CONNECTED",
                409,
                "Transmitter not connected",
                "Transmitter must be connected for factory binding.",
            )
        if self.runtime.receiver_connection.state != ConnectionState.connected:
            raise ApplicationError(
                "RECEIVER_NOT_CONNECTED",
                409,
                "Receiver not connected",
                "Receiver must be connected for factory binding.",
            )
