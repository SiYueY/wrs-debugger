import logging
from collections.abc import Mapping
from uuid import uuid4

from fastapi import Request
from fastapi.exceptions import RequestValidationError
from fastapi.responses import JSONResponse

from wrs_debugger.errors import ApplicationError, JsonScalar
from wrs_debugger.models.error import ProblemDetails, ValidationViolation

logger = logging.getLogger(__name__)

PROBLEM_RESPONSES: dict[int, dict[str, object]] = {
    status: {"model": ProblemDetails}
    for status in (400, 404, 409, 422, 500, 502, 503, 504)
}


def request_id(request: Request) -> str:
    existing = request.headers.get("x-request-id")
    return existing if existing else uuid4().hex


def response(
    error: ApplicationError,
    request: Request,
    *,
    violations: list[ValidationViolation] | None = None,
) -> JSONResponse:
    body = ProblemDetails(
        type=f"urn:wrs-debugger:error:{error.code.lower().replace('_', '-')}",
        title=error.title,
        status=error.status,
        detail=error.detail,
        code=error.code,
        request_id=request_id(request),
        context=error.context,
        violations=violations,
    )
    return JSONResponse(status_code=error.status, content=body.model_dump(mode="json"))


async def application_error_handler(request: Request, error: Exception) -> JSONResponse:
    if isinstance(error, ApplicationError):
        return response(error, request)
    return await unexpected_error_handler(request, error)


async def validation_error_handler(request: Request, error: Exception) -> JSONResponse:
    if not isinstance(error, RequestValidationError):
        return await unexpected_error_handler(request, error)
    violations = [_to_violation(item) for item in error.errors()]
    return response(
        ApplicationError(
            "VALIDATION_FAILED",
            422,
            "Validation failed",
            "The request is invalid.",
        ),
        request,
        violations=violations,
    )


async def unexpected_error_handler(request: Request, error: Exception) -> JSONResponse:
    current_request_id = request_id(request)
    logger.exception(
        "Unhandled backend exception request_id=%s path=%s",
        current_request_id,
        request.url.path,
        exc_info=error,
    )
    body = ProblemDetails(
        type="urn:wrs-debugger:error:internal-error",
        title="Internal error",
        status=500,
        detail="An unexpected error occurred.",
        code="INTERNAL_ERROR",
        request_id=current_request_id,
    )
    return JSONResponse(status_code=500, content=body.model_dump(mode="json"))


def _to_violation(item: Mapping[str, object]) -> ValidationViolation:
    raw_location = item.get("loc")
    if isinstance(raw_location, (list, tuple)):
        parts = [str(part) for part in raw_location if part not in {"body", "query", "path"}]
    else:
        parts = []
    error_type = str(item.get("type", "invalid_value"))
    return ValidationViolation(
        field=".".join(parts),
        code=_validation_code(error_type),
        context=_scalar_context(item.get("ctx")),
    )


def _validation_code(error_type: str) -> str:
    if error_type == "missing":
        return "REQUIRED"
    if error_type in {"greater_than", "greater_than_equal"}:
        return "VALUE_TOO_SMALL"
    if error_type in {"less_than", "less_than_equal"}:
        return "VALUE_TOO_LARGE"
    if error_type in {"string_pattern_mismatch", "string_too_short", "string_too_long"}:
        return "INVALID_FORMAT"
    return "INVALID_VALUE"


def _scalar_context(value: object) -> dict[str, JsonScalar]:
    if not isinstance(value, dict):
        return {}
    result: dict[str, JsonScalar] = {}
    for key, item in value.items():
        if item is None or isinstance(item, (str, int, float, bool)):
            result[str(key)] = item
    return result
