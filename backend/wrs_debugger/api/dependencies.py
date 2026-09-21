from typing import cast

from fastapi import Request

from wrs_debugger.services.binding import FactoryBindingService
from wrs_debugger.services.receiver import ReceiverService
from wrs_debugger.services.synchronization import SynchronizationService
from wrs_debugger.services.transmitter import TransmitterService


async def transmitter_service(request: Request) -> TransmitterService:
    return cast(TransmitterService, request.app.state.transmitter_service)


async def receiver_service(request: Request) -> ReceiverService:
    return cast(ReceiverService, request.app.state.receiver_service)


async def synchronization_service(request: Request) -> SynchronizationService:
    return cast(SynchronizationService, request.app.state.synchronization_service)


async def binding_service(request: Request) -> FactoryBindingService:
    return cast(FactoryBindingService, request.app.state.binding_service)
