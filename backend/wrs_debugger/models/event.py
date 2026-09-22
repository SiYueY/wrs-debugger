from enum import StrEnum
from typing import Annotated, Literal

from pydantic import AwareDatetime, Field

from wrs_debugger.models.base import ApiModel
from wrs_debugger.models.connection import ReceiverConnection, TransmitterConnection
from wrs_debugger.models.device import ReceiverInfo
from wrs_debugger.models.operation import Operation
from wrs_debugger.models.system import BackendPhase


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


class EmptyEventData(ApiModel):
    pass


class PhaseChangedData(ApiModel):
    phase: BackendPhase


class EventBase(ApiModel):
    version: Literal["1.0"] = "1.0"
    stream_id: str
    sequence: int = Field(ge=0)
    timestamp: AwareDatetime


class SystemPhaseChangedEvent(EventBase):
    event: Literal["system.phase.changed"]
    data: PhaseChangedData


class TransmitterConnectionChangedEvent(EventBase):
    event: Literal["transmitter.connection.changed"]
    data: TransmitterConnection


class ReceiverConnectionChangedEvent(EventBase):
    event: Literal["receiver.connection.changed"]
    data: ReceiverConnection


class TransmitterLoRaParametersChangedEvent(EventBase):
    event: Literal["transmitter.lora_parameters.changed"]
    data: EmptyEventData


class TransmitterGfskParametersChangedEvent(EventBase):
    event: Literal["transmitter.gfsk_parameters.changed"]
    data: EmptyEventData


class ReceiverLoRaParametersChangedEvent(EventBase):
    event: Literal["receiver.lora_parameters.changed"]
    data: EmptyEventData


class ReceiverGfskParametersChangedEvent(EventBase):
    event: Literal["receiver.gfsk_parameters.changed"]
    data: EmptyEventData


class ReceiverInfoChangedEvent(EventBase):
    event: Literal["receiver.info.changed"]
    data: ReceiverInfo


class OperationUpdatedEvent(EventBase):
    event: Literal["operation.updated"]
    data: Operation


WebSocketEvent = Annotated[
    SystemPhaseChangedEvent
    | TransmitterConnectionChangedEvent
    | ReceiverConnectionChangedEvent
    | TransmitterLoRaParametersChangedEvent
    | TransmitterGfskParametersChangedEvent
    | ReceiverLoRaParametersChangedEvent
    | ReceiverGfskParametersChangedEvent
    | ReceiverInfoChangedEvent
    | OperationUpdatedEvent,
    Field(discriminator="event"),
]
