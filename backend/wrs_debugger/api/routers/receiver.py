from fastapi import APIRouter, Depends, Response

from wrs_debugger.api.dependencies import binding_service, receiver_service, synchronization_service
from wrs_debugger.api.errors import PROBLEM_RESPONSES
from wrs_debugger.models.connection import (
    ConnectReceiverRequest,
    ReceiverConnection,
    ReceiverSettings,
)
from wrs_debugger.models.device import ReceiverInfo, SdoReadRequest, SdoResponse, SdoWriteRequest
from wrs_debugger.models.gfsk import GfskParameters
from wrs_debugger.models.lora import LoRaParameters
from wrs_debugger.models.operation import OperationCreatedResponse
from wrs_debugger.services.binding import FactoryBindingService
from wrs_debugger.services.receiver import ReceiverService
from wrs_debugger.services.synchronization import SynchronizationService

router = APIRouter(
    prefix="/receiver",
    tags=["receiver"],
    responses=PROBLEM_RESPONSES,
)


@router.get("/settings", response_model=ReceiverSettings, operation_id="get_receiver_settings")
async def get_settings(service: ReceiverService = Depends(receiver_service)) -> ReceiverSettings:
    return service.settings()


@router.put("/settings", response_model=ReceiverSettings, operation_id="update_receiver_settings")
async def update_settings(
    body: ReceiverSettings, service: ReceiverService = Depends(receiver_service)
) -> ReceiverSettings:
    return service.update_settings(body)


@router.get(
    "/connection", response_model=ReceiverConnection, operation_id="get_receiver_connection"
)
async def get_connection(
    service: ReceiverService = Depends(receiver_service),
) -> ReceiverConnection:
    return service.connection()


@router.post("/connect", response_model=ReceiverConnection, operation_id="connect_receiver")
async def connect(
    body: ConnectReceiverRequest, service: ReceiverService = Depends(receiver_service)
) -> ReceiverConnection:
    return await service.connect(body)


@router.post("/disconnect", response_model=ReceiverConnection, operation_id="disconnect_receiver")
async def disconnect(service: ReceiverService = Depends(receiver_service)) -> ReceiverConnection:
    return await service.disconnect()


@router.get("", response_model=ReceiverInfo, operation_id="get_receiver_info")
async def get_info(service: ReceiverService = Depends(receiver_service)) -> ReceiverInfo:
    return await service.info()


@router.post("/sdo/read", response_model=SdoResponse, operation_id="read_receiver_sdo")
async def read_sdo(
    body: SdoReadRequest, service: ReceiverService = Depends(receiver_service)
) -> SdoResponse:
    return await service.read_sdo(body.object_address)


@router.put("/sdo", response_model=SdoResponse, operation_id="write_receiver_sdo")
async def write_sdo(
    body: SdoWriteRequest, service: ReceiverService = Depends(receiver_service)
) -> SdoResponse:
    return await service.write_sdo(body.object_address, body.object_data)


@router.get(
    "/lora-parameters", response_model=LoRaParameters, operation_id="read_receiver_lora_parameters"
)
async def read_lora(service: ReceiverService = Depends(receiver_service)) -> LoRaParameters:
    return await service.read_lora()


@router.put("/lora-parameters", status_code=204, operation_id="write_receiver_lora_parameters")
async def write_lora(
    body: LoRaParameters, service: ReceiverService = Depends(receiver_service)
) -> Response:
    await service.write_lora(body)
    return Response(status_code=204)


@router.post(
    "/lora-parameters/restore-defaults",
    status_code=204,
    operation_id="restore_receiver_lora_defaults",
)
async def restore_lora(service: ReceiverService = Depends(receiver_service)) -> Response:
    await service.restore_lora()
    return Response(status_code=204)


@router.post(
    "/lora-parameters/sync-from-transmitter",
    status_code=202,
    response_model=OperationCreatedResponse,
    operation_id="sync_receiver_lora_parameters",
)
async def sync_lora(
    service: SynchronizationService = Depends(synchronization_service),
) -> OperationCreatedResponse:
    operation = await service.sync_lora()
    return OperationCreatedResponse(operation_id=operation.operation_id)


@router.get(
    "/gfsk-parameters", response_model=GfskParameters, operation_id="read_receiver_gfsk_parameters"
)
async def read_gfsk(service: ReceiverService = Depends(receiver_service)) -> GfskParameters:
    return await service.read_gfsk()


@router.put("/gfsk-parameters", status_code=204, operation_id="write_receiver_gfsk_parameters")
async def write_gfsk(
    body: GfskParameters, service: ReceiverService = Depends(receiver_service)
) -> Response:
    await service.write_gfsk(body)
    return Response(status_code=204)


@router.post(
    "/gfsk-parameters/restore-defaults",
    status_code=204,
    operation_id="restore_receiver_gfsk_defaults",
)
async def restore_gfsk(service: ReceiverService = Depends(receiver_service)) -> Response:
    await service.restore_gfsk()
    return Response(status_code=204)


@router.post(
    "/gfsk-parameters/sync-from-transmitter",
    status_code=202,
    response_model=OperationCreatedResponse,
    operation_id="sync_receiver_gfsk_parameters",
)
async def sync_gfsk(
    service: SynchronizationService = Depends(synchronization_service),
) -> OperationCreatedResponse:
    operation = await service.sync_gfsk()
    return OperationCreatedResponse(operation_id=operation.operation_id)


@router.post(
    "/factory-bind",
    status_code=202,
    response_model=OperationCreatedResponse,
    operation_id="factory_bind_receiver",
)
async def factory_bind(
    service: FactoryBindingService = Depends(binding_service),
) -> OperationCreatedResponse:
    operation = await service.bind()
    return OperationCreatedResponse(operation_id=operation.operation_id)
