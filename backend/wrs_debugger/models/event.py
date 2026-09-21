from datetime import datetime
from enum import StrEnum

from wrs_debugger.models.base import ApiModel


class EventName(StrEnum):
    system_phase_changed = "system.phase.changed"
    transmitter_connection_changed = "transmitter.connection.changed"
    receiver_connection_changed = "receiver.connection.changed"
    transmitter_lora_parameters_changed = "transmitter.lora_parameters.changed"
    transmitter_gfsk_parameters_changed = "transmitter.gfsk_parameters.changed"
    receiver_lora_parameters_changed = "receiver.lora_parameters.changed"
    receiver_gfsk_parameters_changed = "receiver.gfsk_parameters.changed"
    receiver_info_changed = "receiver.info.changed"
    operation_updated = "operation.updated"


class WebSocketEvent(ApiModel):
    version: str = "1.0"
    stream_id: str
    sequence: int
    event: EventName
    timestamp: datetime
    data: dict[str, object]
