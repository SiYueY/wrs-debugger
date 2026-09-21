from wrs_debugger.models.base import ApiModel
from wrs_debugger.models.connection import ReceiverConnection, TransmitterConnection
from wrs_debugger.models.operation import Operation


class HealthResponse(ApiModel):
    status: str
    service: str
    version: str


class SystemInfoResponse(ApiModel):
    backend_version: str
    backend_phase: str


class SnapshotResponse(ApiModel):
    phase: str
    transmitter_connection: TransmitterConnection
    receiver_connection: ReceiverConnection
    active_operation: Operation | None = None
