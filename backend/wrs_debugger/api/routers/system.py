from typing import cast

from fastapi import APIRouter, Request

from wrs_debugger.models.system import HealthResponse, SnapshotResponse, SystemInfoResponse
from wrs_debugger.operations.manager import OperationManager
from wrs_debugger.services.runtime import Runtime

router = APIRouter(tags=["system"])


@router.get("/health", response_model=HealthResponse, operation_id="get_health")
async def get_health() -> HealthResponse:
    return HealthResponse(status="ok", service="wrs-debugger-backend", version="1.0.0")


@router.get("/system/info", response_model=SystemInfoResponse, operation_id="get_system_info")
async def get_system_info() -> SystemInfoResponse:
    return SystemInfoResponse(backend_version="1.0.0", backend_phase="ready")


@router.get("/snapshot", response_model=SnapshotResponse, operation_id="get_snapshot")
async def get_snapshot(request: Request) -> SnapshotResponse:
    runtime = cast(Runtime, request.app.state.runtime)
    operations = cast(OperationManager, request.app.state.operations)
    return SnapshotResponse(
        phase="ready",
        transmitter_connection=runtime.transmitter_connection,
        receiver_connection=runtime.receiver_connection,
        active_operation=operations.active(),
    )
