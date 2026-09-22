from wrs_debugger.models.base import ApiModel


class DiagnosticError(ApiModel):
    """The device's current diagnostic error, if one is active."""

    code: str | None = None
    detail: str | None = None
