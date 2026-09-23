from dataclasses import dataclass, field
from uuid import uuid4

from wrs_debugger.errors import ApplicationError
from wrs_debugger.gateway.base import BindingHandle
from wrs_debugger.models.connection import (
    ConnectionState,
    ReceiverConnection,
    ReceiverSettings,
    TransmitterConnection,
)
from wrs_debugger.models.device import ReceiverInfo, SdoResponse, SerialPortInfo, TransmitterInfo
from wrs_debugger.models.diagnostic import DiagnosticError
from wrs_debugger.models.gfsk import GfskParameters
from wrs_debugger.models.lora import LoRaParameters


def default_lora_parameters() -> LoRaParameters:
    return LoRaParameters(
        param_flags=0,
        tx_power=10,
        freq_offset=250,
        payload_len=12,
        rssi_threshold=110,
        heartbeat_interval=200,
        heartbeat_loss=3,
        bandwidth=1,
        spreading_factor=6,
        coding_rate=4,
        header_type=0,
        preamble_len=12,
        sync_word=5156,
    )


def default_gfsk_parameters() -> GfskParameters:
    return GfskParameters(
        param_flags=16384,
        tx_power=10,
        freq_offset=250,
        payload_len=12,
        rssi_threshold=110,
        heartbeat_interval=200,
        heartbeat_loss=3,
        bandwidth=1,
        bitrate=50000,
        freq_deviation=25000,
        pulse_shaping=9,
        preamble_len=16,
        sync_word=5156,
    )


@dataclass
class MockWrsGateway:
    """Deterministic hardware-free WRS implementation for development and tests."""

    available_serial_ports: list[SerialPortInfo] = field(
        default_factory=lambda: [
            SerialPortInfo(device="/dev/ttyACM0", description="USB ACM serial"),
            SerialPortInfo(device="/dev/ttyUSB0", description="USB serial"),
        ]
    )
    transmitter_info: TransmitterInfo = field(
        default_factory=lambda: TransmitterInfo(
            device_id="A1B2C3", product_code=1001, version_number=1, serial_number=42
        )
    )
    transmitter_pin: str = "123456"
    transmitter_sdo: dict[int, int] = field(
        default_factory=lambda: {0x001: 1001, 0x002: 1, 0x003: 42, 0x102: 100, 0x202: 0}
    )
    diagnostic_error: DiagnosticError = field(default_factory=DiagnosticError)
    transmitter_lora_parameters: LoRaParameters = field(default_factory=default_lora_parameters)
    transmitter_gfsk_parameters: GfskParameters = field(default_factory=default_gfsk_parameters)
    receiver_lora_parameters: LoRaParameters = field(default_factory=default_lora_parameters)
    receiver_gfsk_parameters: GfskParameters = field(default_factory=default_gfsk_parameters)
    receiver_bound_device_id: str | None = None
    transmitter_connection: TransmitterConnection = field(
        default_factory=lambda: TransmitterConnection(state=ConnectionState.disconnected)
    )
    receiver_connection: ReceiverConnection = field(
        default_factory=lambda: ReceiverConnection(state=ConnectionState.disconnected)
    )
    receiver_settings: ReceiverSettings = field(
        default_factory=lambda: ReceiverSettings(domain_id=0)
    )
    binding_handle: BindingHandle | None = None
    fail_transmitter_connect: bool = False
    fail_receiver_connect: bool = False
    fail_factory_bind: bool = False
    fail_binding_receiver: bool = False

    def list_serial_ports(self) -> list[SerialPortInfo]:
        return [port.model_copy(deep=True) for port in self.available_serial_ports]

    def read_diagnostic_error(self) -> DiagnosticError:
        return self.diagnostic_error.model_copy(deep=True)

    def clear_diagnostic_error(self) -> None:
        self.diagnostic_error = DiagnosticError()

    def connect_transmitter(self, device: str) -> None:
        if not any(port.device == device for port in self.available_serial_ports):
            raise ApplicationError(
                "SERIAL_PORT_NOT_FOUND",
                404,
                "Serial port not found",
                "The selected serial port is unavailable.",
                {"device": device},
            )
        if self.fail_transmitter_connect:
            raise ApplicationError(
                "TRANSMITTER_CONNECTION_FAILED",
                503,
                "Transmitter connection failed",
                "Mock transmitter connection failed.",
            )

    def disconnect_transmitter(self) -> None:
        return

    def probe_transmitter(self) -> None:
        return

    def read_transmitter_sdo(self, object_address: int) -> SdoResponse:
        return SdoResponse(
            object_address=object_address,
            object_data=self.transmitter_sdo.get(object_address, 0),
            status=0x4,
            result_code=0,
        )

    def write_transmitter_sdo(self, object_address: int, object_data: int) -> SdoResponse:
        self.transmitter_sdo[object_address] = object_data
        return SdoResponse(object_address=object_address, object_data=0, status=0x6, result_code=0)

    def read_transmitter_info(self) -> TransmitterInfo:
        return self.transmitter_info.model_copy(deep=True)

    def read_transmitter_pin(self) -> str:
        return self.transmitter_pin

    def write_transmitter_pin(self, pin: str) -> None:
        self.transmitter_pin = pin

    def read_transmitter_lora_parameters(self) -> LoRaParameters:
        return self.transmitter_lora_parameters.model_copy(deep=True)

    def write_transmitter_lora_parameters(self, parameters: LoRaParameters) -> None:
        self.transmitter_lora_parameters = parameters.model_copy(deep=True)

    def restore_transmitter_lora_defaults(self) -> None:
        self.transmitter_lora_parameters = default_lora_parameters()

    def read_transmitter_gfsk_parameters(self) -> GfskParameters:
        return self.transmitter_gfsk_parameters.model_copy(deep=True)

    def write_transmitter_gfsk_parameters(self, parameters: GfskParameters) -> None:
        self.transmitter_gfsk_parameters = parameters.model_copy(deep=True)

    def restore_transmitter_gfsk_defaults(self) -> None:
        self.transmitter_gfsk_parameters = default_gfsk_parameters()

    def connect_receiver(self, domain_id: int) -> None:
        if self.fail_receiver_connect:
            raise ApplicationError(
                "RECEIVER_CONNECTION_FAILED",
                503,
                "Receiver connection failed",
                "Mock receiver connection failed.",
                {"domain_id": domain_id},
            )

    def disconnect_receiver(self) -> None:
        return

    def read_receiver_info(self) -> ReceiverInfo:
        return ReceiverInfo(bound_device_id=self.receiver_bound_device_id)

    def read_receiver_lora_parameters(self) -> LoRaParameters:
        return self.receiver_lora_parameters.model_copy(deep=True)

    def write_receiver_lora_parameters(self, parameters: LoRaParameters) -> None:
        self.receiver_lora_parameters = parameters.model_copy(deep=True)

    def restore_receiver_lora_defaults(self) -> None:
        self.receiver_lora_parameters = default_lora_parameters()

    def read_receiver_gfsk_parameters(self) -> GfskParameters:
        return self.receiver_gfsk_parameters.model_copy(deep=True)

    def write_receiver_gfsk_parameters(self, parameters: GfskParameters) -> None:
        self.receiver_gfsk_parameters = parameters.model_copy(deep=True)

    def restore_receiver_gfsk_defaults(self) -> None:
        self.receiver_gfsk_parameters = default_gfsk_parameters()

    def prepare_transmitter_binding(self) -> BindingHandle:
        if self.fail_factory_bind:
            raise ApplicationError("BINDING_FAILED", 502, "Binding failed", "Mock binding failed.")
        self.binding_handle = BindingHandle(uuid4().hex)
        return self.binding_handle

    def prepare_receiver_binding(self, handle: BindingHandle) -> None:
        if self.binding_handle is not handle:
            raise ApplicationError(
                "BINDING_FAILED", 502, "Binding failed", "Invalid binding handle."
            )
        if self.fail_binding_receiver:
            raise ApplicationError(
                "BINDING_FAILED", 502, "Binding failed", "Mock receiver binding failed."
            )

    def verify_binding(self, handle: BindingHandle) -> None:
        if self.binding_handle is not handle:
            raise ApplicationError(
                "BINDING_FAILED", 502, "Binding failed", "Invalid binding handle."
            )
        self.receiver_bound_device_id = self.transmitter_info.device_id
        self.binding_handle = None

    def rollback_binding(self, handle: BindingHandle) -> None:
        if self.binding_handle is handle:
            self.binding_handle = None
            self.receiver_bound_device_id = None

    def record_transmitter_connection(self, connection: TransmitterConnection) -> None:
        self.transmitter_connection = connection.model_copy(deep=True)

    def record_receiver_connection(self, connection: ReceiverConnection) -> None:
        self.receiver_connection = connection.model_copy(deep=True)

    def record_receiver_settings(self, settings: ReceiverSettings) -> None:
        self.receiver_settings = settings.model_copy(deep=True)
