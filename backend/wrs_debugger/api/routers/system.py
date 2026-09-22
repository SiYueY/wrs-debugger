from typing import cast

from fastapi import APIRouter, Request

from wrs_debugger import __version__
from wrs_debugger.api.errors import PROBLEM_RESPONSES
from wrs_debugger.models.system import (
    HealthResponse,
    ReceiverSnapshot,
    SnapshotResponse,
    SystemInfoResponse,
    TransmitterSnapshot,
)
from wrs_debugger.operations.manager import OperationManager
from wrs_debugger.services.runtime import Runtime
from wrs_debugger.settings import BackendSettings

router = APIRouter(tags=["system"], responses=PROBLEM_RESPONSES)


@router.get("/health", response_model=HealthResponse, operation_id="get_health")
async def get_health(request: Request) -> HealthResponse:
    runtime = cast(Runtime, request.app.state.runtime)
    return HealthResponse(
        status="ok",
        ready=runtime.phase.value == "ready_for_control",
        phase=runtime.phase,
    )


@router.get("/system/info", response_model=SystemInfoResponse, operation_id="get_system_info")
async def get_system_info(request: Request) -> SystemInfoResponse:
    settings = cast(BackendSettings, request.app.state.backend_settings)
    return SystemInfoResponse(
        product_version=__version__,
        backend_version=__version__,
        backend_build_id=settings.build_id,
        api_version="v1",
        event_version="1.0",
        backend_instance_id=cast(str, request.app.state.backend_instance_id),
    )


@router.get("/snapshot", response_model=SnapshotResponse, operation_id="get_snapshot")
async def get_snapshot(request: Request) -> SnapshotResponse:
    runtime = cast(Runtime, request.app.state.runtime)
    operations = cast(OperationManager, request.app.state.operations)
    return SnapshotResponse(
        phase=runtime.phase,
        transmitter=TransmitterSnapshot(connection=runtime.transmitter_connection),
        receiver=ReceiverSnapshot(connection=runtime.receiver_connection),
        active_operation=operations.active(),
    )
