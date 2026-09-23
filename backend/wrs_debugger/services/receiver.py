from datetime import UTC, datetime

from wrs_debugger.errors import ApplicationError
from wrs_debugger.models.connection import (
    ConnectionError,
    ConnectionState,
    ConnectReceiverRequest,
    ReceiverConnection,
    ReceiverSettings,
)
from wrs_debugger.models.device import ReceiverInfo
from wrs_debugger.models.event import EventName
from wrs_debugger.models.gfsk import GfskParameters
from wrs_debugger.models.lora import LoRaParameters
from wrs_debugger.services.runtime import Runtime
from wrs_debugger.settings import SettingsRepository


class ReceiverService:
    def __init__(self, runtime: Runtime, settings: SettingsRepository) -> None:
        self.runtime = runtime
        self.settings_repository = settings
        self._settings = ReceiverSettings(domain_id=settings.load_domain_id())

    def settings(self) -> ReceiverSettings:
        return self._settings.model_copy(deep=True)

    def update_settings(self, settings: ReceiverSettings) -> ReceiverSettings:
        try:
            self.settings_repository.save_domain_id(settings.domain_id)
        except OSError as error:
            raise ApplicationError(
                "SETTINGS_WRITE_FAILED",
                500,
                "Settings write failed",
                "Receiver settings could not be persisted.",
            ) from error
        self._settings = settings.model_copy(deep=True)
        self.runtime.record_receiver_settings(settings)
        return self.settings()

    def connection(self) -> ReceiverConnection:
        return self.runtime.receiver_connection.model_copy(deep=True)

    async def connect(self, request: ConnectReceiverRequest) -> ReceiverConnection:
        self.runtime.require_no_cross_device_operation()
        async with self.runtime.receiver_lock:
            current = self.runtime.receiver_connection
            if (
                current.state == ConnectionState.connected
                and current.domain_id == request.domain_id
            ):
                return self.connection()
            if current.state == ConnectionState.connected:
                await self.runtime.call(self.runtime.gateway.disconnect_receiver)
            self.runtime.set_receiver_connection(
                ReceiverConnection(state=ConnectionState.connecting, domain_id=request.domain_id)
            )
            await self._publish_connection()
            try:
                await self.runtime.call(
                    lambda: self.runtime.gateway.connect_receiver(request.domain_id)
                )
            except ApplicationError as error:
                await self._set_failed_connection(request.domain_id, error.code, error.detail)
                raise
            except Exception:
                await self._set_failed_connection(
                    request.domain_id,
                    "INTERNAL_ERROR",
                    "An unexpected error occurred while connecting the receiver.",
                )
                raise
            self.runtime.set_receiver_connection(
                ReceiverConnection(
                    state=ConnectionState.connected,
                    domain_id=request.domain_id,
                    connected_at=datetime.now(UTC),
                )
            )
            await self._publish_connection()
            return self.connection()

    async def disconnect(self) -> ReceiverConnection:
        self.runtime.require_no_cross_device_operation()
        async with self.runtime.receiver_lock:
            if self.runtime.receiver_connection.state != ConnectionState.connected:
                self.runtime.set_receiver_connection(
                    ReceiverConnection(state=ConnectionState.disconnected)
                )
                await self._publish_connection()
                return self.connection()
            try:
                await self.runtime.call(self.runtime.gateway.disconnect_receiver)
            except ApplicationError as error:
                if error.code not in {"RECEIVER_DISCONNECTED", "RECEIVER_NOT_CONNECTED"}:
                    await self._set_failed_connection(
                        self.runtime.receiver_connection.domain_id, error.code, error.detail
                    )
                    raise
            except Exception:
                await self._set_failed_connection(
                    self.runtime.receiver_connection.domain_id,
                    "INTERNAL_ERROR",
                    "An unexpected error occurred while disconnecting the receiver.",
                )
                raise
            self.runtime.set_receiver_connection(
                ReceiverConnection(state=ConnectionState.disconnected)
            )
            await self._publish_connection()
            return self.connection()

    async def info(self) -> ReceiverInfo:
        self._require_connected()
        async with self.runtime.receiver_lock:
            return await self.runtime.call_receiver(self.runtime.gateway.read_receiver_info)

    async def read_lora(self) -> LoRaParameters:
        self._require_connected()
        async with self.runtime.receiver_lock:
            return await self.runtime.call_receiver(
                self.runtime.gateway.read_receiver_lora_parameters
            )

    async def write_lora(self, parameters: LoRaParameters) -> None:
        self._require_connected()
        async with self.runtime.receiver_lock:
            await self.runtime.call_receiver(
                lambda: self.runtime.gateway.write_receiver_lora_parameters(parameters)
            )
        await self.runtime.publish(EventName.receiver_lora_parameters_changed, {})

    async def restore_lora(self) -> None:
        self._require_connected()
        async with self.runtime.receiver_lock:
            await self.runtime.call_receiver(self.runtime.gateway.restore_receiver_lora_defaults)
        await self.runtime.publish(EventName.receiver_lora_parameters_changed, {})

    async def read_gfsk(self) -> GfskParameters:
        self._require_connected()
        async with self.runtime.receiver_lock:
            return await self.runtime.call_receiver(
                self.runtime.gateway.read_receiver_gfsk_parameters
            )

    async def write_gfsk(self, parameters: GfskParameters) -> None:
        self._require_connected()
        async with self.runtime.receiver_lock:
            await self.runtime.call_receiver(
                lambda: self.runtime.gateway.write_receiver_gfsk_parameters(parameters)
            )
        await self.runtime.publish(EventName.receiver_gfsk_parameters_changed, {})

    async def restore_gfsk(self) -> None:
        self._require_connected()
        async with self.runtime.receiver_lock:
            await self.runtime.call_receiver(self.runtime.gateway.restore_receiver_gfsk_defaults)
        await self.runtime.publish(EventName.receiver_gfsk_parameters_changed, {})

    def _require_connected(self) -> None:
        self.runtime.require_no_cross_device_operation()
        if self.runtime.receiver_connection.state != ConnectionState.connected:
            raise ApplicationError(
                "RECEIVER_NOT_CONNECTED",
                409,
                "Receiver not connected",
                "Receiver must be connected for this operation.",
            )

    async def _set_failed_connection(self, domain_id: int | None, code: str, detail: str) -> None:
        self.runtime.set_receiver_connection(
            ReceiverConnection(
                state=ConnectionState.failed,
                domain_id=domain_id,
                error=ConnectionError(code=code, detail=detail),
            )
        )
        await self._publish_connection()

    async def _publish_connection(self) -> None:
        await self.runtime.publish(
            EventName.receiver_connection_changed,
            self.runtime.receiver_connection.model_dump(mode="json"),
        )
