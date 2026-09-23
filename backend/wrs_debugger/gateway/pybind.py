"""Native transmitter gateway backed by the C++ pybind11 extension."""

from typing import Any, Never

from wrs_debugger.errors import ApplicationError
from wrs_debugger.gateway.base import BindingHandle, WrsGateway
from wrs_debugger.models.device import ReceiverInfo, SdoResponse, SerialPortInfo, TransmitterInfo
from wrs_debugger.models.diagnostic import DiagnosticError
from wrs_debugger.models.gfsk import GfskParameters
from wrs_debugger.models.lora import LoRaParameters
from wrs_debugger.services.native_executor import ThreadedNativeExecutor

_ERRORS: dict[str, tuple[str, int, str]] = {
    "TRANSMITTER_3": ("TRANSMITTER_NOT_CONNECTED", 409, "Transmitter is not connected."),
    "TRANSMITTER_5": ("SERIAL_PORT_NOT_FOUND", 404, "The selected serial device was not found."),
    "TRANSMITTER_6": (
        "SERIAL_PERMISSION_DENIED",
        403,
        "Permission was denied for the serial device.",
    ),
    "TRANSMITTER_7": ("SERIAL_PORT_BUSY", 409, "The serial device is already in use."),
    "TRANSMITTER_8": ("TRANSMITTER_DISCONNECTED", 503, "The transmitter disconnected."),
    "TRANSMITTER_9": ("TRANSMITTER_TIMEOUT", 504, "The transmitter did not respond in time."),
    "TRANSMITTER_11": (
        "TRANSMITTER_PROTOCOL_MISMATCH",
        502,
        "The device is not a compatible transmitter.",
    ),
    "TRANSMITTER_13": (
        "TRANSMITTER_UNEXPECTED_RESPONSE",
        502,
        "The transmitter sent an unexpected response.",
    ),
    "TRANSMITTER_14": (
        "TRANSMITTER_CRC_MISMATCH",
        502,
        "The transmitter response failed CRC validation.",
    ),
    "TRANSMITTER_16": ("TRANSMITTER_REJECTED", 502, "The transmitter rejected the request."),
    "TRANSMITTER_17": ("SDO_NOT_RECEIVED", 502, "The transmitter did not receive the SDO request."),
    "TRANSMITTER_18": ("SDO_IN_PROGRESS", 409, "The transmitter is still processing the SDO request."),
    "TRANSMITTER_19": ("SDO_ERROR", 502, "The transmitter rejected the SDO operation."),
    "TRANSMITTER_20": ("SDO_INVALID_COMMAND", 422, "The transmitter rejected the SDO command."),
}


def native_available() -> bool:
    try:
        import wrs_debugger_native  # type: ignore[import-not-found]  # noqa: F401
    except ImportError:
        return False
    return True


class PybindWrsGateway:
    """Production gateway. Receiver operations are explicitly unavailable in V1."""

    def __init__(self) -> None:
        try:
            import wrs_debugger_native
        except ImportError as error:
            raise RuntimeError("WRS native transmitter module is unavailable") from error
        self._native: Any = wrs_debugger_native
        self._client: Any = wrs_debugger_native.TransmitterClient()

    @staticmethod
    def native_executor() -> ThreadedNativeExecutor:
        return ThreadedNativeExecutor()

    def _call(self, function: Any) -> Any:
        try:
            return function()
        except ApplicationError:
            raise
        except (RuntimeError, ValueError) as error:
            code, status, detail = _ERRORS.get(
                str(error), ("TRANSMITTER_IO_ERROR", 502, "The transmitter operation failed.")
            )
            raise ApplicationError(code, status, "Transmitter operation failed", detail) from error

    def list_serial_ports(self) -> list[SerialPortInfo]:
        return [
            SerialPortInfo.model_validate(item)
            for item in self._call(self._native.list_serial_ports)
        ]

    def connect_transmitter(self, device: str) -> None:
        self._call(lambda: self._client.open(device))

    def disconnect_transmitter(self) -> None:
        self._call(self._client.close)

    def probe_transmitter(self) -> None:
        # identity() is cached after open(), so it cannot detect a later cable/PTY loss.
        # PIN read is a harmless request that requires a live protocol round-trip.
        self._call(self._client.read_pin)

    def read_transmitter_sdo(self, object_address: int) -> SdoResponse:
        return SdoResponse.model_validate(self._call(lambda: self._client.read_sdo(object_address)))

    def write_transmitter_sdo(self, object_address: int, object_data: int) -> SdoResponse:
        self._call(lambda: self._client.write_sdo(object_address, object_data))
        return SdoResponse(
            object_address=object_address, object_data=0, status=0x6, result_code=0
        )

    def read_transmitter_info(self) -> TransmitterInfo:
        identity = self._call(self._client.identity)
        return TransmitterInfo(
            device_id=str(self._call(self._client.read_device_id)),
            product_code=identity["product_code"],
            version_number=identity["version_number"],
            serial_number=identity["serial_number"],
        )

    def read_transmitter_pin(self) -> str:
        return str(self._call(self._client.read_pin))

    def write_transmitter_pin(self, pin: str) -> None:
        self._call(lambda: self._client.write_pin(pin))

    def read_transmitter_lora_parameters(self) -> LoRaParameters:
        return LoRaParameters.model_validate(self._call(self._client.read_lora))

    def write_transmitter_lora_parameters(self, parameters: LoRaParameters) -> None:
        self._call(lambda: self._client.write_lora(parameters.model_dump()))

    def restore_transmitter_lora_defaults(self) -> None:
        self._call(self._client.restore_lora)

    def read_transmitter_gfsk_parameters(self) -> GfskParameters:
        return GfskParameters.model_validate(self._call(self._client.read_gfsk))

    def write_transmitter_gfsk_parameters(self, parameters: GfskParameters) -> None:
        self._call(lambda: self._client.write_gfsk(parameters.model_dump()))

    def restore_transmitter_gfsk_defaults(self) -> None:
        self._call(self._client.restore_gfsk)

    def _receiver_unavailable(self) -> Never:
        raise ApplicationError(
            "RECEIVER_UNAVAILABLE",
            503,
            "Receiver unavailable",
            "Native Receiver DDS support is not available.",
        )

    def read_diagnostic_error(self) -> DiagnosticError:
        self._receiver_unavailable()

    def clear_diagnostic_error(self) -> None:
        self._receiver_unavailable()

    def connect_receiver(self, domain_id: int) -> None:
        self._receiver_unavailable()

    def disconnect_receiver(self) -> None:
        self._receiver_unavailable()

    def read_receiver_info(self) -> ReceiverInfo:
        self._receiver_unavailable()

    def read_receiver_lora_parameters(self) -> LoRaParameters:
        self._receiver_unavailable()

    def write_receiver_lora_parameters(self, parameters: LoRaParameters) -> None:
        self._receiver_unavailable()

    def restore_receiver_lora_defaults(self) -> None:
        self._receiver_unavailable()

    def read_receiver_gfsk_parameters(self) -> GfskParameters:
        self._receiver_unavailable()

    def write_receiver_gfsk_parameters(self, parameters: GfskParameters) -> None:
        self._receiver_unavailable()

    def restore_receiver_gfsk_defaults(self) -> None:
        self._receiver_unavailable()

    def prepare_transmitter_binding(self) -> BindingHandle:
        self._receiver_unavailable()

    def prepare_receiver_binding(self, handle: BindingHandle) -> None:
        self._receiver_unavailable()

    def verify_binding(self, handle: BindingHandle) -> None:
        self._receiver_unavailable()

    def rollback_binding(self, handle: BindingHandle) -> None:
        self._receiver_unavailable()


def pybind_gateway_contract(_: WrsGateway) -> None:
    """Type-check helper documenting that the adapter implements WrsGateway."""
