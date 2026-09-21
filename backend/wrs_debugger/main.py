from collections.abc import AsyncIterator
from contextlib import asynccontextmanager

from fastapi import FastAPI, WebSocket
from fastapi.exceptions import RequestValidationError

from wrs_debugger.api.errors import (
    application_error_handler,
    unexpected_error_handler,
    validation_error_handler,
)
from wrs_debugger.api.routers import operations, receiver, system, transmitter
from wrs_debugger.errors import ApplicationError
from wrs_debugger.gateway.mock import MockWrsGateway
from wrs_debugger.models.connection import (
    ConnectionState,
    ReceiverConnection,
    TransmitterConnection,
)
from wrs_debugger.operations.manager import OperationManager
from wrs_debugger.services.binding import FactoryBindingService
from wrs_debugger.services.native_executor import InlineMockExecutor
from wrs_debugger.services.receiver import ReceiverService
from wrs_debugger.services.runtime import Runtime
from wrs_debugger.services.synchronization import SynchronizationService
from wrs_debugger.services.transmitter import TransmitterService
from wrs_debugger.settings import SettingsRepository
from wrs_debugger.websocket.hub import WebSocketHub


@asynccontextmanager
async def lifespan(app: FastAPI) -> AsyncIterator[None]:
    hub = WebSocketHub()
    gateway = MockWrsGateway()
    runtime = Runtime(
        gateway=gateway,
        publish=hub.publish,
        transmitter_connection=TransmitterConnection(state=ConnectionState.disconnected),
        receiver_connection=ReceiverConnection(state=ConnectionState.disconnected),
        executor=InlineMockExecutor(),
        state_mirror=gateway,
    )
    operation_manager = OperationManager(hub.publish, gateway.record_active_operation)
    runtime.operation_active = lambda: operation_manager.active() is not None
    app.state.gateway = gateway
    app.state.runtime = runtime
    app.state.operations = operation_manager
    app.state.transmitter_service = TransmitterService(runtime)
    app.state.receiver_service = ReceiverService(runtime, SettingsRepository())
    app.state.synchronization_service = SynchronizationService(runtime, operation_manager)
    app.state.binding_service = FactoryBindingService(runtime, operation_manager)
    app.state.websocket_hub = hub
    try:
        yield
    finally:
        await operation_manager.shutdown()


app = FastAPI(title="WRS Debugger API", version="1.0.0", lifespan=lifespan)
app.add_exception_handler(ApplicationError, application_error_handler)
app.add_exception_handler(RequestValidationError, validation_error_handler)
app.add_exception_handler(Exception, unexpected_error_handler)
app.include_router(system.router, prefix="/api/v1")
app.include_router(transmitter.router, prefix="/api/v1")
app.include_router(receiver.router, prefix="/api/v1")
app.include_router(operations.router, prefix="/api/v1")


@app.websocket("/api/v1/events")
async def events(socket: WebSocket) -> None:
    await socket.app.state.websocket_hub.serve(socket)
