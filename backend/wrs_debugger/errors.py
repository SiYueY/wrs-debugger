from dataclasses import dataclass, field


@dataclass(slots=True)
class ApplicationError(Exception):
    code: str
    status: int
    title: str
    detail: str
    context: dict[str, object] = field(default_factory=dict)


def invalid_argument(detail: str, **context: object) -> ApplicationError:
    return ApplicationError("INVALID_ARGUMENT", 400, "Invalid argument", detail, context)


def not_connected(device: str) -> ApplicationError:
    return ApplicationError(
        f"{device.upper()}_NOT_CONNECTED",
        409,
        f"{device.title()} not connected",
        f"{device.title()} must be connected for this operation.",
    )
