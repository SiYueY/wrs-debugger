from enum import StrEnum

from pydantic import BaseModel, Field


class ConnectionState(StrEnum):
    disconnected = "disconnected"
    connecting = "connecting"
    connected = "connected"
    failed = "failed"


class RobotConnection(BaseModel):
    ip: str
    domain_id: int = Field(ge=0)
    state: ConnectionState


class BoxConnection(BaseModel):
    device: str | None = None
    state: ConnectionState


class ConnectRobotRequest(BaseModel):
    ip: str
    domain_id: int = Field(ge=0)


class ConnectBoxRequest(BaseModel):
    device: str


class PinRequest(BaseModel):
    pin: str = Field(pattern=r"^[0-9]{6}$")


class BoxInfo(BaseModel):
    device_id: str
    connection_state: ConnectionState


class ReceiverInfo(BaseModel):
    robot_connection_state: ConnectionState
    bound_device_id: str | None


class OperationType(StrEnum):
    sync_receiver_config = "sync_receiver_config"
    factory_bind = "factory_bind"


class OperationState(StrEnum):
    pending = "pending"
    running = "running"
    succeeded = "succeeded"
    failed = "failed"


class Operation(BaseModel):
    id: str
    type: OperationType
    state: OperationState
    stage: str
    progress: int
    message: str
    error: str | None = None


class OperationCreated(BaseModel):
    operation_id: str


class ErrorResponse(BaseModel):
    code: str
    message: str


class EventType(StrEnum):
    robot_connection_changed = "robot_connection_changed"
    box_connection_changed = "box_connection_changed"
    operation_updated = "operation_updated"


EventData = RobotConnection | BoxConnection | Operation


class Event(BaseModel):
    type: EventType
    data: EventData
