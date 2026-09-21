from wrs_debugger.models.base import ApiModel
from wrs_debugger.models.numeric import Int16, Uint8, Uint16


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
