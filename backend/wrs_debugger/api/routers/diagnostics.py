from fastapi import APIRouter, Depends, Response, status

from wrs_debugger.api.dependencies import diagnostics_service
from wrs_debugger.api.errors import PROBLEM_RESPONSES
from wrs_debugger.models.diagnostic import DiagnosticError
from wrs_debugger.services.diagnostics import DiagnosticsService

router = APIRouter(prefix="/diagnostics", tags=["diagnostics"], responses=PROBLEM_RESPONSES)


@router.get("/error", response_model=DiagnosticError, operation_id="get_diagnostic_error")
async def get_diagnostic_error(
    service: DiagnosticsService = Depends(diagnostics_service),
) -> DiagnosticError:
    return await service.read_error()


@router.post(
    "/error/clear",
    status_code=status.HTTP_204_NO_CONTENT,
    operation_id="clear_diagnostic_error",
)
async def clear_diagnostic_error(
    service: DiagnosticsService = Depends(diagnostics_service),
) -> Response:
    await service.clear_error()
    return Response(status_code=status.HTTP_204_NO_CONTENT)
