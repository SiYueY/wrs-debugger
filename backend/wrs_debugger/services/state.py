import asyncio
from dataclasses import dataclass, field

from ..models.contracts import BoxConnection, ConnectionState, Operation, RobotConnection
from ..models.radio import DEFAULT_RADIO_CONFIG, RadioConfig


@dataclass
class MockState:
    robot: RobotConnection = field(
        default_factory=lambda: RobotConnection(
            ip="192.168.1.10", domain_id=0, state=ConnectionState.disconnected
        )
    )
    box: BoxConnection = field(
        default_factory=lambda: BoxConnection(state=ConnectionState.disconnected)
    )
    box_config: RadioConfig = field(
        default_factory=lambda: DEFAULT_RADIO_CONFIG.model_copy(deep=True)
    )
    receiver_config: RadioConfig = field(
        default_factory=lambda: DEFAULT_RADIO_CONFIG.model_copy(deep=True)
    )
    operations: dict[str, Operation] = field(default_factory=dict)
    tasks: set[asyncio.Task[None]] = field(default_factory=set)
