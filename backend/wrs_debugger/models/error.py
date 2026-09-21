from pydantic import Field

from wrs_debugger.models.base import ApiModel

JsonScalar = str | int | float | bool | None


class ValidationViolation(ApiModel):
    field: str
    code: str
    context: dict[str, JsonScalar] = Field(default_factory=dict)


class ProblemDetails(ApiModel):
    type: str
    title: str
    status: int
    detail: str
    code: str
    request_id: str
    context: dict[str, JsonScalar] = Field(default_factory=dict)
    violations: list[ValidationViolation] | None = None
