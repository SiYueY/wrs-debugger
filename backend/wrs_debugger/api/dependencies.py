from typing import cast

from fastapi import Request

from wrs_debugger.services.binding import FactoryBindingService
from wrs_debugger.services.diagnostics import DiagnosticsService
from wrs_debugger.services.receiver import ReceiverService
from wrs_debugger.services.synchronization import SynchronizationService
from wrs_debugger.services.transmitter import TransmitterService


def transmitter_service(request: Request) -> TransmitterService:
    return cast(TransmitterService, request.app.state.transmitter_service)


def receiver_service(request: Request) -> ReceiverService:
    return cast(ReceiverService, request.app.state.receiver_service)


def synchronization_service(request: Request) -> SynchronizationService:
    return cast(SynchronizationService, request.app.state.synchronization_service)


def binding_service(request: Request) -> FactoryBindingService:
    return cast(FactoryBindingService, request.app.state.binding_service)


def diagnostics_service(request: Request) -> DiagnosticsService:
    return cast(DiagnosticsService, request.app.state.diagnostics_service)
