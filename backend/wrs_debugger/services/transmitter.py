from datetime import UTC, datetime

from wrs_debugger.errors import ApplicationError
from wrs_debugger.models.connection import (
    ConnectionError,
    ConnectionState,
    ConnectTransmitterRequest,
    TransmitterConnection,
)
from wrs_debugger.models.device import (
    PinResponse,
    SdoResponse,
    SerialPortListResponse,
    TransmitterInfo,
    WritePinRequest,
)
from wrs_debugger.models.event import EventName
from wrs_debugger.models.gfsk import GfskParameters
from wrs_debugger.models.lora import LoRaParameters
from wrs_debugger.services.runtime import Runtime


class TransmitterService:
    def __init__(self, runtime: Runtime) -> None:
        self.runtime = runtime

    async def list_serial_ports(self) -> SerialPortListResponse:
        ports = await self.runtime.call(self.runtime.gateway.list_serial_ports)
        return SerialPortListResponse(ports=ports)

    def connection(self) -> TransmitterConnection:
        return self.runtime.transmitter_connection.model_copy(deep=True)

    async def connect(self, request: ConnectTransmitterRequest) -> TransmitterConnection:
        self.runtime.require_no_cross_device_operation()
        async with self.runtime.transmitter_lock:
            current = self.runtime.transmitter_connection
            if current.state == ConnectionState.connected and current.device == request.device:
                return current.model_copy(deep=True)
            if current.state == ConnectionState.connected:
                await self.runtime.call(self.runtime.gateway.disconnect_transmitter)
            self.runtime.set_transmitter_connection(
                TransmitterConnection(state=ConnectionState.connecting, device=request.device)
            )
            await self._publish_connection()
            try:
                await self.runtime.call(
                    lambda: self.runtime.gateway.connect_transmitter(request.device)
                )
            except ApplicationError as error:
                await self._set_failed_connection(request.device, error.code, error.detail)
                raise
            except Exception:
                await self._set_failed_connection(
                    request.device,
                    "INTERNAL_ERROR",
                    "An unexpected error occurred while connecting the transmitter.",
                )
                raise
            self.runtime.set_transmitter_connection(
                TransmitterConnection(
                    state=ConnectionState.connected,
                    device=request.device,
                    connected_at=datetime.now(UTC),
                )
            )
            await self._publish_connection()
            return self.connection()

    async def disconnect(self) -> TransmitterConnection:
        self.runtime.require_no_cross_device_operation()
        async with self.runtime.transmitter_lock:
            if self.runtime.transmitter_connection.state != ConnectionState.connected:
                self.runtime.set_transmitter_connection(
                    TransmitterConnection(state=ConnectionState.disconnected)
                )
                await self._publish_connection()
                return self.connection()
            try:
                await self.runtime.call(self.runtime.gateway.disconnect_transmitter)
            except ApplicationError as error:
                if error.code not in {"TRANSMITTER_DISCONNECTED", "TRANSMITTER_NOT_CONNECTED"}:
                    await self._set_failed_connection(
                        self.runtime.transmitter_connection.device, error.code, error.detail
                    )
                    raise
            except Exception:
                await self._set_failed_connection(
                    self.runtime.transmitter_connection.device,
                    "INTERNAL_ERROR",
                    "An unexpected error occurred while disconnecting the transmitter.",
                )
                raise
            self.runtime.set_transmitter_connection(
                TransmitterConnection(state=ConnectionState.disconnected)
            )
            await self._publish_connection()
            return self.connection()

    async def probe_connection(self) -> None:
        """Perform one protocol round-trip when a transmitter is connected."""
        if not self._is_connected():
            return
        async with self.runtime.transmitter_lock:
            # The connection may have changed while the monitor waited for an operation.
            if not self._is_connected():
                return
            await self.runtime.call_transmitter(self.runtime.gateway.probe_transmitter)

    async def info(self) -> TransmitterInfo:
        self._require_connected()
        async with self.runtime.transmitter_lock:
            return await self.runtime.call_transmitter(self.runtime.gateway.read_transmitter_info)

    async def read_sdo(self, object_address: int) -> SdoResponse:
        self._require_connected()
        async with self.runtime.transmitter_lock:
            return await self.runtime.call_transmitter(
                lambda: self.runtime.gateway.read_transmitter_sdo(object_address)
            )

    async def write_sdo(self, object_address: int, object_data: int) -> SdoResponse:
        self._require_connected()
        if object_address != 0x202:
            raise ApplicationError(
                "SDO_READ_ONLY", 403, "SDO object is read-only", "Only object 0X202 is writable."
            )
        if object_data != 0x454E:
            raise ApplicationError(
                "SDO_INVALID_VALUE", 422, "Invalid SDO value", "Object 0X202 requires 0X0000454E."
            )
        async with self.runtime.transmitter_lock:
            return await self.runtime.call_transmitter(
                lambda: self.runtime.gateway.write_transmitter_sdo(object_address, object_data)
            )

    async def read_pin(self) -> PinResponse:
        self._require_connected()
        async with self.runtime.transmitter_lock:
            return PinResponse(
                pin=await self.runtime.call_transmitter(self.runtime.gateway.read_transmitter_pin)
            )

    async def write_pin(self, request: WritePinRequest) -> None:
        self._require_connected()
        if request.pin == "000000":
            raise ApplicationError(
                "PARAMETER_VALIDATION_FAILED",
                422,
                "Parameter validation failed",
                "PIN 000000 is reserved for the protocol read request.",
            )
        async with self.runtime.transmitter_lock:
            await self.runtime.call_transmitter(
                lambda: self.runtime.gateway.write_transmitter_pin(request.pin)
            )

    async def read_lora(self) -> LoRaParameters:
        self._require_connected()
        async with self.runtime.transmitter_lock:
            return await self.runtime.call_transmitter(
                self.runtime.gateway.read_transmitter_lora_parameters
            )

    async def write_lora(self, parameters: LoRaParameters) -> None:
        self._require_connected()
        async with self.runtime.transmitter_lock:
            await self.runtime.call_transmitter(
                lambda: self.runtime.gateway.write_transmitter_lora_parameters(parameters)
            )
        await self.runtime.publish(EventName.transmitter_lora_parameters_changed, {})

    async def restore_lora(self) -> None:
        self._require_connected()
        async with self.runtime.transmitter_lock:
            await self.runtime.call_transmitter(
                self.runtime.gateway.restore_transmitter_lora_defaults
            )
        await self.runtime.publish(EventName.transmitter_lora_parameters_changed, {})

    async def read_gfsk(self) -> GfskParameters:
        self._require_connected()
        async with self.runtime.transmitter_lock:
            return await self.runtime.call_transmitter(
                self.runtime.gateway.read_transmitter_gfsk_parameters
            )

    async def write_gfsk(self, parameters: GfskParameters) -> None:
        self._require_connected()
        async with self.runtime.transmitter_lock:
            await self.runtime.call_transmitter(
                lambda: self.runtime.gateway.write_transmitter_gfsk_parameters(parameters)
            )
        await self.runtime.publish(EventName.transmitter_gfsk_parameters_changed, {})

    async def restore_gfsk(self) -> None:
        self._require_connected()
        async with self.runtime.transmitter_lock:
            await self.runtime.call_transmitter(
                self.runtime.gateway.restore_transmitter_gfsk_defaults
            )
        await self.runtime.publish(EventName.transmitter_gfsk_parameters_changed, {})

    def _require_connected(self) -> None:
        self.runtime.require_no_cross_device_operation()
        if not self._is_connected():
            raise ApplicationError(
                "TRANSMITTER_NOT_CONNECTED",
                409,
                "Transmitter not connected",
                "Transmitter must be connected for this operation.",
            )

    def _is_connected(self) -> bool:
        return self.runtime.transmitter_connection.state == ConnectionState.connected

    async def _set_failed_connection(self, device: str | None, code: str, detail: str) -> None:
        self.runtime.set_transmitter_connection(
            TransmitterConnection(
                state=ConnectionState.failed,
                device=device,
                error=ConnectionError(code=code, detail=detail),
            )
        )
        await self._publish_connection()

    async def _publish_connection(self) -> None:
        await self.runtime.publish(
            EventName.transmitter_connection_changed,
            self.runtime.transmitter_connection.model_dump(mode="json"),
        )
