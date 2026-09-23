from datetime import UTC, datetime

from wrs_debugger.errors import ApplicationError
from wrs_debugger.models.connection import (
    ConnectionError,
    ConnectionState,
    ConnectReceiverRequest,
    ReceiverConnection,
    ReceiverSettings,
)
from wrs_debugger.models.device import ReceiverInfo, SdoResponse
from wrs_debugger.models.event import EventName
from wrs_debugger.models.gfsk import GfskParameters
from wrs_debugger.models.lora import LoRaParameters
from wrs_debugger.services.runtime import Runtime
from wrs_debugger.settings import SettingsRepository


class ReceiverService:
    _SDO_READABLE = frozenset({0x001, 0x002, 0x003, *range(0x008, 0x044), 0x201, 0x202})
    _SDO_WRITABLE = frozenset({0x202})
    _UPGRADE_REQUEST_VALUE = 0x0000454E

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

    async def read_sdo(self, object_address: int) -> SdoResponse:
        self._require_connected()
        if object_address not in self._SDO_READABLE:
            raise ApplicationError(
                "RECEIVER_SDO_INVALID_ADDRESS",
                422,
                "Invalid receiver SDO address",
                "The object address is reserved or undefined in the Sub_1G object dictionary.",
                {"object_address": object_address},
            )
        async with self.runtime.receiver_lock:
            return await self.runtime.call_receiver(
                lambda: self.runtime.gateway.read_receiver_sdo(object_address)
            )

    async def write_sdo(self, object_address: int, object_data: int) -> SdoResponse:
        self._require_connected()
        if object_address not in self._SDO_WRITABLE:
            raise ApplicationError(
                "RECEIVER_SDO_READ_ONLY",
                422,
                "Receiver SDO is read-only",
                "Only Object 0X202 is writable in the Sub_1G object dictionary.",
                {"object_address": object_address},
            )
        if object_data != self._UPGRADE_REQUEST_VALUE:
            raise ApplicationError(
                "RECEIVER_SDO_INVALID_VALUE",
                422,
                "Invalid receiver SDO value",
                "Object 0X202 requires 0X0000454E.",
                {"object_address": object_address},
            )
        async with self.runtime.receiver_lock:
            return await self.runtime.call_receiver(
                lambda: self.runtime.gateway.write_receiver_sdo(object_address, object_data)
            )

    async def read_lora(self) -> LoRaParameters:
        self._require_connected()
        async with self.runtime.receiver_lock:
            parameters = await self.runtime.call_receiver(
                self.runtime.gateway.read_receiver_lora_parameters
            )
            self._validate_lora(parameters, response=True)
            return parameters

    async def write_lora(self, parameters: LoRaParameters) -> None:
        self._require_connected()
        self._validate_lora(parameters)
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
            parameters = await self.runtime.call_receiver(
                self.runtime.gateway.read_receiver_gfsk_parameters
            )
            self._validate_gfsk(parameters, response=True)
            return parameters

    async def write_gfsk(self, parameters: GfskParameters) -> None:
        self._require_connected()
        self._validate_gfsk(parameters)
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

    @staticmethod
    def _validation_error(field: str, response: bool) -> None:
        if response:
            raise ApplicationError(
                "RECEIVER_INVALID_RESPONSE",
                502,
                "Invalid receiver response",
                f"Receiver returned an invalid {field} value.",
                {"field": field},
            )
        raise ApplicationError(
            "PARAMETER_VALIDATION_FAILED",
            422,
            "Parameter validation failed",
            f"Receiver parameter {field} violates the communication protocol.",
            {"field": field},
        )

    @classmethod
    def _validate_common(
        cls,
        parameters: LoRaParameters | GfskParameters,
        expected_type: int,
        response: bool,
    ) -> None:
        flags = parameters.param_flags
        if flags & 0x3FC0 or (flags >> 14) != expected_type or flags & 0x0010:
            cls._validation_error("param_flags", response)
        max_power = 20 if flags & 1 else 10
        if not 0 <= parameters.tx_power <= max_power:
            cls._validation_error("tx_power", response)
        if parameters.payload_len != 12:
            cls._validation_error("payload_len", response)
        if not 10 <= parameters.rssi_threshold <= 148:
            cls._validation_error("rssi_threshold", response)
        if not 200 <= parameters.heartbeat_interval <= 10000:
            cls._validation_error("heartbeat_interval", response)
        if not 1 <= parameters.heartbeat_loss <= 255:
            cls._validation_error("heartbeat_loss", response)
        if parameters.bandwidth > 2:
            cls._validation_error("bandwidth", response)

    @classmethod
    def _validate_lora(cls, parameters: LoRaParameters, response: bool = False) -> None:
        cls._validate_common(parameters, 0, response)
        if not 5 <= parameters.spreading_factor <= 12:
            cls._validation_error("spreading_factor", response)
        if parameters.coding_rate > 6:
            cls._validation_error("coding_rate", response)
        if parameters.header_type > 1:
            cls._validation_error("header_type", response)
        if not 10 <= parameters.preamble_len <= 50 or (
            parameters.spreading_factor in {5, 6} and parameters.preamble_len != 12
        ):
            cls._validation_error("preamble_len", response)
        if parameters.sync_word & 0x0F0F != 0x0404:
            cls._validation_error("sync_word", response)

    @classmethod
    def _validate_gfsk(cls, parameters: GfskParameters, response: bool = False) -> None:
        cls._validate_common(parameters, 1, response)
        if not 600 <= parameters.bitrate <= 150000:
            cls._validation_error("bitrate", response)
        if not 600 <= parameters.freq_deviation <= 300000 or (
            4 * parameters.freq_deviation < parameters.bitrate
        ):
            cls._validation_error("freq_deviation", response)
        if parameters.pulse_shaping not in {0, 8, 9, 10, 11}:
            cls._validation_error("pulse_shaping", response)
        if parameters.preamble_len < 16:
            cls._validation_error("preamble_len", response)

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
