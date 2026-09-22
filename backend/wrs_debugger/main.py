from collections.abc import AsyncIterator, Callable
from contextlib import asynccontextmanager
from uuid import uuid4

from fastapi import FastAPI, WebSocket
from fastapi.exceptions import RequestValidationError

from wrs_debugger import __version__
from wrs_debugger.api.errors import (
    application_error_handler,
    unexpected_error_handler,
    validation_error_handler,
)
from wrs_debugger.api.routers import operations, receiver, system, transmitter
from wrs_debugger.errors import ApplicationError
from wrs_debugger.gateway.base import WrsGateway
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
from wrs_debugger.services.runtime import Runtime, StateMirror
from wrs_debugger.services.synchronization import SynchronizationService
from wrs_debugger.services.transmitter import TransmitterService
from wrs_debugger.settings import BackendSettings, SettingsRepository
from wrs_debugger.websocket.hub import WebSocketHub

GatewayFactory = Callable[[], WrsGateway]


def create_app(
    *,
    gateway_factory: GatewayFactory | None = None,
    settings_repository: SettingsRepository | None = None,
    backend_settings: BackendSettings | None = None,
) -> FastAPI:
    factory = gateway_factory or MockWrsGateway
    repository = settings_repository or SettingsRepository()
    configured_settings = backend_settings or BackendSettings()

    @asynccontextmanager
    async def lifespan(application: FastAPI) -> AsyncIterator[None]:
        hub = WebSocketHub()
        gateway = factory()
        state_mirror = gateway if isinstance(gateway, StateMirror) else None
        runtime = Runtime(
            gateway=gateway,
            publish=hub.publish,
            transmitter_connection=TransmitterConnection(state=ConnectionState.disconnected),
            receiver_connection=ReceiverConnection(state=ConnectionState.disconnected),
            executor=InlineMockExecutor(),
            state_mirror=state_mirror,
        )
        operation_manager = OperationManager(hub.publish)
        runtime.operation_active = lambda: operation_manager.active() is not None
        application.state.gateway = gateway
        application.state.runtime = runtime
        application.state.operations = operation_manager
        application.state.transmitter_service = TransmitterService(runtime)
        application.state.receiver_service = ReceiverService(runtime, repository)
        application.state.synchronization_service = SynchronizationService(
            runtime, operation_manager
        )
        application.state.binding_service = FactoryBindingService(runtime, operation_manager)
        application.state.websocket_hub = hub
        application.state.backend_settings = configured_settings
        application.state.backend_instance_id = uuid4().hex
        try:
            yield
        finally:
            await operation_manager.shutdown()

    application = FastAPI(title="WRS Debugger API", version=__version__, lifespan=lifespan)
    application.add_exception_handler(ApplicationError, application_error_handler)
    application.add_exception_handler(RequestValidationError, validation_error_handler)
    application.add_exception_handler(Exception, unexpected_error_handler)
    application.include_router(system.router, prefix="/api/v1")
    application.include_router(transmitter.router, prefix="/api/v1")
    application.include_router(receiver.router, prefix="/api/v1")
    application.include_router(operations.router, prefix="/api/v1")

    @application.websocket("/api/v1/events")
    async def events(socket: WebSocket) -> None:
        await socket.app.state.websocket_hub.serve(socket)

    return application


app = create_app()
