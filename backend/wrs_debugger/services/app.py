import asyncio
from collections.abc import Awaitable, Callable
from uuid import uuid4

from fastapi import HTTPException

from ..models.contracts import (
    BoxConnection,
    BoxInfo,
    ConnectBoxRequest,
    ConnectionState,
    ConnectRobotRequest,
    EventData,
    EventType,
    Operation,
    OperationState,
    OperationType,
    PinRequest,
    ReceiverInfo,
    RobotConnection,
)
from ..models.radio import DEFAULT_RADIO_CONFIG, RadioConfig
from .state import MockState

Notifier = Callable[[EventType, EventData], Awaitable[None]]


class DebuggerService:
    def __init__(self, state: MockState, notify: Notifier) -> None:
        self.state = state
        self.notify = notify

    def require_robot(self) -> None:
        if self.state.robot.state != ConnectionState.connected:
            raise HTTPException(
                400, {"code": "robot_not_connected", "message": "Robot is not connected"}
            )

    def require_box(self) -> None:
        if self.state.box.state != ConnectionState.connected:
            raise HTTPException(
                400,
                {"code": "box_not_connected", "message": "Wireless E-Stop Box is not connected"},
            )

    def robot_connection(self) -> RobotConnection:
        return self.state.robot

    async def apply_robot_connection(self, request: ConnectRobotRequest) -> RobotConnection:
        self.state.robot.ip = request.ip
        self.state.robot.domain_id = request.domain_id
        await self.notify(EventType.robot_connection_changed, self.state.robot)
        return self.state.robot

    def box_connection(self) -> BoxConnection:
        return self.state.box

    def serial_devices(self) -> list[str]:
        return ["/dev/ttyACM0", "/dev/ttyUSB0"]

    def box_info(self) -> BoxInfo:
        return BoxInfo(device_id="A1B2C3", connection_state=self.state.box.state)

    def receiver_info(self) -> ReceiverInfo:
        return ReceiverInfo(robot_connection_state=self.state.robot.state, bound_device_id="A1B2C3")

    def set_pin(self, _: PinRequest) -> None:
        self.require_box()

    def box_config(self) -> RadioConfig:
        self.require_box()
        return self.state.box_config

    def receiver_config(self) -> RadioConfig:
        self.require_robot()
        return self.state.receiver_config

    def set_box_config(self, config: RadioConfig) -> RadioConfig:
        self.require_box()
        self.state.box_config = config
        return config

    def set_receiver_config(self, config: RadioConfig) -> RadioConfig:
        self.require_robot()
        self.state.receiver_config = config
        return config

    def restore_box_defaults(self) -> RadioConfig:
        self.require_box()
        self.state.box_config = DEFAULT_RADIO_CONFIG.model_copy(deep=True)
        return self.state.box_config

    def restore_receiver_defaults(self) -> RadioConfig:
        self.require_robot()
        self.state.receiver_config = DEFAULT_RADIO_CONFIG.model_copy(deep=True)
        return self.state.receiver_config

    def get_operation(self, operation_id: str) -> Operation:
        operation = self.state.operations.get(operation_id)
        if operation is None:
            raise HTTPException(
                404, {"code": "operation_not_found", "message": "Operation was not found"}
            )
        return operation

    async def connect_robot(self, request: ConnectRobotRequest) -> RobotConnection:
        self.state.robot = RobotConnection(
            ip=request.ip, domain_id=request.domain_id, state=ConnectionState.connecting
        )
        await self.notify(EventType.robot_connection_changed, self.state.robot)
        await asyncio.sleep(0.3)
        self.state.robot.state = ConnectionState.connected
        await self.notify(EventType.robot_connection_changed, self.state.robot)
        return self.state.robot

    async def disconnect_robot(self) -> RobotConnection:
        self.state.robot.state = ConnectionState.disconnected
        await self.notify(EventType.robot_connection_changed, self.state.robot)
        return self.state.robot

    async def connect_box(self, request: ConnectBoxRequest) -> BoxConnection:
        self.state.box = BoxConnection(device=request.device, state=ConnectionState.connecting)
        await self.notify(EventType.box_connection_changed, self.state.box)
        await asyncio.sleep(0.3)
        self.state.box.state = ConnectionState.connected
        await self.notify(EventType.box_connection_changed, self.state.box)
        return self.state.box

    async def disconnect_box(self) -> BoxConnection:
        self.state.box.state = ConnectionState.disconnected
        await self.notify(EventType.box_connection_changed, self.state.box)
        return self.state.box

    def new_operation(self, type_: OperationType) -> Operation:
        if any(
            item.state in {OperationState.pending, OperationState.running}
            for item in self.state.operations.values()
        ):
            raise HTTPException(
                409, {"code": "operation_conflict", "message": "Another operation is running"}
            )
        operation = Operation(
            id=uuid4().hex,
            type=type_,
            state=OperationState.pending,
            stage="pending",
            progress=0,
            message="Pending",
        )
        self.state.operations[operation.id] = operation
        task = asyncio.create_task(self.run_operation(operation))
        self.state.tasks.add(task)
        task.add_done_callback(self.state.tasks.discard)
        return operation

    async def run_operation(self, operation: Operation) -> None:
        try:
            stages = (
                ["reading_box_config", "writing_receiver_config", "completed"]
                if operation.type == OperationType.sync_receiver_config
                else ["prepare_box", "configure_receiver", "verify_wireless_link", "completed"]
            )
            for index, stage in enumerate(stages):
                operation.state = OperationState.running
                operation.stage = stage
                operation.progress = round((index + 1) * 100 / len(stages))
                operation.message = stage.replace("_", " ").title()
                await self.notify(EventType.operation_updated, operation)
                await asyncio.sleep(0.1)
            if operation.type == OperationType.sync_receiver_config:
                self.state.receiver_config = self.state.box_config.model_copy(deep=True)
            operation.state = OperationState.succeeded
            await self.notify(EventType.operation_updated, operation)
        except asyncio.CancelledError:
            operation.state = OperationState.failed
            operation.error = "Operation cancelled"
            await self.notify(EventType.operation_updated, operation)
            raise
        except Exception:
            operation.state = OperationState.failed
            operation.error = "Mock operation failed"
            await self.notify(EventType.operation_updated, operation)
