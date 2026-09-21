from uuid import uuid4

from fastapi import Request
from fastapi.exceptions import RequestValidationError
from fastapi.responses import JSONResponse

from wrs_debugger.errors import ApplicationError
from wrs_debugger.models.error import ProblemDetails


def request_id(request: Request) -> str:
    existing = request.headers.get("x-request-id")
    return existing if existing else uuid4().hex


def response(error: ApplicationError, request: Request) -> JSONResponse:
    body = ProblemDetails(
        type=f"urn:wrs-debugger:error:{error.code.lower().replace('_', '-')}",
        title=error.title,
        status=error.status,
        detail=error.detail,
        code=error.code,
        request_id=request_id(request),
        context=error.context,
    )
    return JSONResponse(status_code=error.status, content=body.model_dump(mode="json"))


async def application_error_handler(request: Request, error: Exception) -> JSONResponse:
    if isinstance(error, ApplicationError):
        return response(error, request)
    return await unexpected_error_handler(request, error)


async def validation_error_handler(request: Request, error: Exception) -> JSONResponse:
    if not isinstance(error, RequestValidationError):
        return await unexpected_error_handler(request, error)
    return response(
        ApplicationError(
            "VALIDATION_FAILED",
            422,
            "Validation failed",
            "The request body is invalid.",
            {"errors": error.errors()},
        ),
        request,
    )


async def unexpected_error_handler(request: Request, _: Exception) -> JSONResponse:
    return response(
        ApplicationError("INTERNAL_ERROR", 500, "Internal error", "An unexpected error occurred."),
        request,
    )
