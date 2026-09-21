from wrs_debugger.models.base import ApiModel


class ProblemDetails(ApiModel):
    type: str
    title: str
    status: int
    detail: str
    code: str
    request_id: str
    context: dict[str, object]
