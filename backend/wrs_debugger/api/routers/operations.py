from typing import cast

from fastapi import APIRouter, Request

from wrs_debugger.api.errors import PROBLEM_RESPONSES
from wrs_debugger.models.operation import ActiveOperationResponse, Operation
from wrs_debugger.operations.manager import OperationManager

router = APIRouter(
    prefix="/operations",
    tags=["operations"],
    responses=PROBLEM_RESPONSES,
)


def manager(request: Request) -> OperationManager:
    return cast(OperationManager, request.app.state.operations)


@router.get("/active", response_model=ActiveOperationResponse, operation_id="get_active_operation")
async def get_active_operation(request: Request) -> ActiveOperationResponse:
    return ActiveOperationResponse(operation=manager(request).active())


@router.get("/{operation_id}", response_model=Operation, operation_id="get_operation")
async def get_operation(operation_id: str, request: Request) -> Operation:
    return manager(request).get(operation_id)
