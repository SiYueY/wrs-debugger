from enum import StrEnum

from pydantic import AwareDatetime, Field

from wrs_debugger.models.base import ApiModel
from wrs_debugger.models.error import JsonScalar


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


class OperationStage(StrEnum):
    pending = "pending"
    reading_transmitter_lora = "reading_transmitter_lora"
    writing_receiver_lora = "writing_receiver_lora"
    reading_transmitter_gfsk = "reading_transmitter_gfsk"
    writing_receiver_gfsk = "writing_receiver_gfsk"
    preparing_transmitter = "preparing_transmitter"
    binding_receiver = "binding_receiver"
    verifying_binding = "verifying_binding"
    rolling_back = "rolling_back"
    completed = "completed"


class OperationFailure(ApiModel):
    code: str
    detail: str
    context: dict[str, JsonScalar] = Field(default_factory=dict)


class Operation(ApiModel):
    operation_id: str
    type: OperationType
    state: OperationState
    stage: OperationStage
    progress: float = Field(ge=0.0, le=1.0)
    started_at: AwareDatetime | None = None
    updated_at: AwareDatetime
    error: OperationFailure | None = None


class OperationCreatedResponse(ApiModel):
    operation_id: str


class ActiveOperationResponse(ApiModel):
    operation: Operation | None = None
