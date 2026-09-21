from datetime import datetime
from enum import StrEnum

from pydantic import Field

from wrs_debugger.models.base import ApiModel


class OperationType(StrEnum):
    sync_lora_parameters = "sync_lora_parameters"
    sync_gfsk_parameters = "sync_gfsk_parameters"
    factory_binding = "factory_binding"


class OperationState(StrEnum):
    pending = "pending"
    running = "running"
    succeeded = "succeeded"
    failed = "failed"
    interrupted = "interrupted"
    unknown = "unknown"


class Operation(ApiModel):
    operation_id: str
    type: OperationType
    state: OperationState
    stage: str
    progress: float = Field(ge=0.0, le=1.0)
    started_at: datetime | None = None
    updated_at: datetime
    error: str | None = None


class OperationCreatedResponse(ApiModel):
    operation_id: str


class ActiveOperationResponse(ApiModel):
    operation: Operation | None = None
