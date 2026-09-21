from enum import StrEnum

from pydantic import AwareDatetime, Field

from wrs_debugger.models.base import ApiModel


class ConnectionState(StrEnum):
    disconnected = "disconnected"
    connecting = "connecting"
    connected = "connected"
    failed = "failed"


class ConnectionError(ApiModel):
    code: str
    detail: str


class TransmitterConnection(ApiModel):
    state: ConnectionState
    device: str | None = None
    connected_at: AwareDatetime | None = None
    error: ConnectionError | None = None


class ReceiverConnection(ApiModel):
    state: ConnectionState
    domain_id: int | None = Field(default=None, ge=0)
    connected_at: AwareDatetime | None = None
    error: ConnectionError | None = None


class ConnectTransmitterRequest(ApiModel):
    device: str = Field(min_length=1)


class ConnectReceiverRequest(ApiModel):
    domain_id: int = Field(ge=0)


class ReceiverSettings(ApiModel):
    domain_id: int = Field(ge=0)
