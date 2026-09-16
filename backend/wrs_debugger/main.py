import asyncio
from collections.abc import AsyncIterator
from contextlib import asynccontextmanager
from typing import cast

from fastapi import FastAPI, HTTPException, Request, WebSocket, WebSocketDisconnect
from fastapi.responses import JSONResponse

from .models.contracts import (
    BoxConnection,
    BoxInfo,
    ConnectBoxRequest,
    ConnectRobotRequest,
    Operation,
    OperationCreated,
    OperationType,
    PinRequest,
    ReceiverInfo,
    RobotConnection,
)
from .models.radio import RadioConfig
from .services.app import DebuggerService
from .services.state import MockState
from .websocket.events import ConnectionManager


@asynccontextmanager
async def lifespan(app: FastAPI) -> AsyncIterator[None]:
    manager = ConnectionManager()
    state = MockState()
    app.state.service = DebuggerService(state, manager.broadcast)
    app.state.manager = manager
    try:
        yield
    finally:
        tasks = tuple(state.tasks)
        for task in tasks:
            task.cancel()
        if tasks:
            await asyncio.gather(*tasks, return_exceptions=True)


app = FastAPI(title="WRS Debugger API", version="0.1.0", lifespan=lifespan)


@app.exception_handler(HTTPException)
async def business_error_handler(_: Request, exc: HTTPException) -> JSONResponse:
    detail: object = exc.detail
    if isinstance(detail, dict) and "code" in detail:
        return JSONResponse(status_code=exc.status_code, content=detail)
    return JSONResponse(
        status_code=exc.status_code, content={"code": "invalid_state", "message": str(exc.detail)}
    )


def service(request: Request) -> DebuggerService:
    return cast(DebuggerService, request.app.state.service)


@app.get("/api/v1/health")
async def health() -> dict[str, str]:
    return {"status": "ok", "service": "wrs-debugger-backend", "version": "0.1.0"}


@app.get("/api/v1/connection/robot")
async def get_robot(request: Request) -> RobotConnection:
    return service(request).robot_connection()


@app.post("/api/v1/connection/robot/connect")
async def connect_robot(body: ConnectRobotRequest, request: Request) -> RobotConnection:
    return await service(request).connect_robot(body)


@app.post("/api/v1/connection/robot/disconnect")
async def disconnect_robot(request: Request) -> RobotConnection:
    return await service(request).disconnect_robot()


@app.post("/api/v1/connection/robot/apply")
async def apply_robot(body: ConnectRobotRequest, request: Request) -> RobotConnection:
    return await service(request).apply_robot_connection(body)


@app.post("/api/v1/connection/robot/reload")
async def reload_robot(request: Request) -> RobotConnection:
    return service(request).robot_connection()


@app.get("/api/v1/connection/box")
async def get_box_connection(request: Request) -> BoxConnection:
    return service(request).box_connection()


@app.get("/api/v1/connection/box/serial-devices")
async def serial_devices(request: Request) -> list[str]:
    return service(request).serial_devices()


@app.post("/api/v1/connection/box/connect")
async def connect_box(body: ConnectBoxRequest, request: Request) -> BoxConnection:
    return await service(request).connect_box(body)


@app.post("/api/v1/connection/box/disconnect")
async def disconnect_box(request: Request) -> BoxConnection:
    return await service(request).disconnect_box()


@app.get("/api/v1/box", response_model=BoxInfo)
async def box_info(request: Request) -> BoxInfo:
    return service(request).box_info()


@app.get("/api/v1/box/config", response_model=RadioConfig)
async def get_box_config(request: Request) -> RadioConfig:
    s = service(request)
    return s.box_config()


@app.put("/api/v1/box/config", response_model=RadioConfig)
async def set_box_config(body: RadioConfig, request: Request) -> RadioConfig:
    s = service(request)
    return s.set_box_config(body)


@app.put("/api/v1/box/pin", status_code=204)
async def set_pin(body: PinRequest, request: Request) -> None:
    service(request).set_pin(body)


@app.post("/api/v1/box/restore-defaults", response_model=RadioConfig)
async def restore_box(request: Request) -> RadioConfig:
    s = service(request)
    return s.restore_box_defaults()


@app.get("/api/v1/receiver", response_model=ReceiverInfo)
async def receiver_info(request: Request) -> ReceiverInfo:
    return service(request).receiver_info()


@app.get("/api/v1/receiver/config", response_model=RadioConfig)
async def get_receiver_config(request: Request) -> RadioConfig:
    s = service(request)
    return s.receiver_config()


@app.put("/api/v1/receiver/config", response_model=RadioConfig)
async def set_receiver_config(body: RadioConfig, request: Request) -> RadioConfig:
    s = service(request)
    return s.set_receiver_config(body)


@app.post("/api/v1/receiver/restore-defaults", response_model=RadioConfig)
async def restore_receiver(request: Request) -> RadioConfig:
    s = service(request)
    return s.restore_receiver_defaults()


@app.post("/api/v1/receiver/sync-from-box", status_code=202, response_model=OperationCreated)
async def sync_receiver(request: Request) -> OperationCreated:
    s = service(request)
    s.require_box()
    s.require_robot()
    item = s.new_operation(OperationType.sync_receiver_config)
    return OperationCreated(operation_id=item.id)


@app.post("/api/v1/receiver/factory-bind", status_code=202, response_model=OperationCreated)
async def bind_receiver(request: Request) -> OperationCreated:
    s = service(request)
    s.require_box()
    s.require_robot()
    item = s.new_operation(OperationType.factory_bind)
    return OperationCreated(operation_id=item.id)


@app.get("/api/v1/operations/{operation_id}")
async def get_operation(operation_id: str, request: Request) -> Operation:
    return service(request).get_operation(operation_id)


@app.websocket("/api/v1/events")
async def events(socket: WebSocket) -> None:
    manager = cast(ConnectionManager, socket.app.state.manager)
    await manager.connect(socket)
    try:
        while True:
            await socket.receive_text()
    except WebSocketDisconnect:
        manager.disconnect(socket)
