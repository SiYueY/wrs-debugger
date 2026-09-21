from collections.abc import Awaitable, Callable

from wrs_debugger.errors import ApplicationError
from wrs_debugger.models.connection import ConnectionState
from wrs_debugger.models.event import EventName
from wrs_debugger.models.operation import Operation, OperationType
from wrs_debugger.operations.manager import OperationManager
from wrs_debugger.services.runtime import Runtime

ProgressUpdate = Callable[[str, float], Awaitable[None]]


class SynchronizationService:
    def __init__(self, runtime: Runtime, operations: OperationManager) -> None:
        self.runtime = runtime
        self.operations = operations

    async def sync_lora(self) -> Operation:
        self._require_connections()

        async def work(update: ProgressUpdate) -> None:
            async with self.runtime.transmitter_lock:
                parameters = await self.runtime.call(
                    self.runtime.gateway.read_transmitter_lora_parameters
                )
                await update("reading_transmitter_lora", 0.35)
                async with self.runtime.receiver_lock:
                    await self.runtime.call(
                        lambda: self.runtime.gateway.write_receiver_lora_parameters(parameters)
                    )
                await update("writing_receiver_lora", 0.8)
            await self.runtime.publish(
                EventName.receiver_lora_parameters_changed, parameters.model_dump(mode="json")
            )

        return await self.operations.start(OperationType.sync_lora_parameters, work)

    async def sync_gfsk(self) -> Operation:
        self._require_connections()

        async def work(update: ProgressUpdate) -> None:
            async with self.runtime.transmitter_lock:
                parameters = await self.runtime.call(
                    self.runtime.gateway.read_transmitter_gfsk_parameters
                )
                await update("reading_transmitter_gfsk", 0.35)
                async with self.runtime.receiver_lock:
                    await self.runtime.call(
                        lambda: self.runtime.gateway.write_receiver_gfsk_parameters(parameters)
                    )
                await update("writing_receiver_gfsk", 0.8)
            await self.runtime.publish(
                EventName.receiver_gfsk_parameters_changed, parameters.model_dump(mode="json")
            )

        return await self.operations.start(OperationType.sync_gfsk_parameters, work)

    def _require_connections(self) -> None:
        if self.runtime.transmitter_connection.state != ConnectionState.connected:
            raise ApplicationError(
                "TRANSMITTER_NOT_CONNECTED",
                409,
                "Transmitter not connected",
                "Transmitter must be connected for synchronization.",
            )
        if self.runtime.receiver_connection.state != ConnectionState.connected:
            raise ApplicationError(
                "RECEIVER_NOT_CONNECTED",
                409,
                "Receiver not connected",
                "Receiver must be connected for synchronization.",
            )
