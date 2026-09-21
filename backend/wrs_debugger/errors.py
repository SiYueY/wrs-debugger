from dataclasses import dataclass, field

JsonScalar = str | int | float | bool | None


@dataclass(slots=True)
class ApplicationError(Exception):
    code: str
    status: int
    title: str
    detail: str
    context: dict[str, JsonScalar] = field(default_factory=dict)
