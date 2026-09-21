from typing import Annotated

from pydantic import Field

from wrs_debugger.models.base import ApiModel

Uint8 = Annotated[int, Field(ge=0, le=255)]
Uint16 = Annotated[int, Field(ge=0, le=65535)]
Uint32 = Annotated[int, Field(ge=0, le=4294967295)]
Int16 = Annotated[int, Field(ge=-32768, le=32767)]


class LoRaParameters(ApiModel):
    param_flags: Uint16
    tx_power: Int16
    freq_offset: Uint16
    payload_len: Uint8
    rssi_threshold: Uint8
    heartbeat_interval: Uint16
    heartbeat_loss: Uint8
    bandwidth: Uint8
    spreading_factor: Uint8
    coding_rate: Uint8
    header_type: Uint8
    preamble_len: Uint8
    sync_word: Uint16


class GfskParameters(ApiModel):
    param_flags: Uint16
    tx_power: Int16
    freq_offset: Uint16
    payload_len: Uint8
    rssi_threshold: Uint8
    heartbeat_interval: Uint16
    heartbeat_loss: Uint8
    bandwidth: Uint8
    bitrate: Uint32
    freq_deviation: Uint32
    pulse_shaping: Uint8
    preamble_len: Uint8
    sync_word: Uint16


DEFAULT_LORA_PARAMETERS = LoRaParameters(
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
DEFAULT_GFSK_PARAMETERS = GfskParameters(
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
