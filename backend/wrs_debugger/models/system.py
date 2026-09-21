from enum import StrEnum
from typing import Literal

from wrs_debugger.models.base import ApiModel
from wrs_debugger.models.connection import ReceiverConnection, TransmitterConnection
from wrs_debugger.models.operation import Operation


class BackendPhase(StrEnum):
    starting = "starting"
    ready_for_control = "ready_for_control"


class HealthResponse(ApiModel):
    status: Literal["ok"]
    ready: bool
    phase: BackendPhase


class SystemInfoResponse(ApiModel):
    product_version: str
    backend_version: str
    backend_build_id: str
    api_version: str
    event_version: str
    backend_instance_id: str


class TransmitterSnapshot(ApiModel):
    connection: TransmitterConnection


class ReceiverSnapshot(ApiModel):
    connection: ReceiverConnection


class SnapshotResponse(ApiModel):
    phase: BackendPhase
    transmitter: TransmitterSnapshot
    receiver: ReceiverSnapshot
    active_operation: Operation | None = None
