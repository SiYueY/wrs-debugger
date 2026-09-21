from fastapi import APIRouter, Depends, Response

from wrs_debugger.api.dependencies import transmitter_service
from wrs_debugger.api.errors import PROBLEM_RESPONSES
from wrs_debugger.models.connection import ConnectTransmitterRequest, TransmitterConnection
from wrs_debugger.models.device import (
    PinResponse,
    SerialPortListResponse,
    TransmitterInfo,
    WritePinRequest,
)
from wrs_debugger.models.gfsk import GfskParameters
from wrs_debugger.models.lora import LoRaParameters
from wrs_debugger.services.transmitter import TransmitterService

router = APIRouter(
    prefix="/transmitter",
    tags=["transmitter"],
    responses=PROBLEM_RESPONSES,
)


@router.get(
    "/serial-ports",
    response_model=SerialPortListResponse,
    operation_id="list_transmitter_serial_ports",
)
async def list_serial_ports(
    service: TransmitterService = Depends(transmitter_service),
) -> SerialPortListResponse:
    return await service.list_serial_ports()


@router.get(
    "/connection", response_model=TransmitterConnection, operation_id="get_transmitter_connection"
)
async def get_connection(
    service: TransmitterService = Depends(transmitter_service),
) -> TransmitterConnection:
    return service.connection()


@router.post("/connect", response_model=TransmitterConnection, operation_id="connect_transmitter")
async def connect(
    body: ConnectTransmitterRequest, service: TransmitterService = Depends(transmitter_service)
) -> TransmitterConnection:
    return await service.connect(body)


@router.post(
    "/disconnect", response_model=TransmitterConnection, operation_id="disconnect_transmitter"
)
async def disconnect(
    service: TransmitterService = Depends(transmitter_service),
) -> TransmitterConnection:
    return await service.disconnect()


@router.get("", response_model=TransmitterInfo, operation_id="get_transmitter_info")
async def get_info(service: TransmitterService = Depends(transmitter_service)) -> TransmitterInfo:
    return await service.info()


@router.get("/pin", response_model=PinResponse, operation_id="read_transmitter_pin")
async def read_pin(service: TransmitterService = Depends(transmitter_service)) -> PinResponse:
    return await service.read_pin()


@router.put("/pin", status_code=204, operation_id="write_transmitter_pin")
async def write_pin(
    body: WritePinRequest, service: TransmitterService = Depends(transmitter_service)
) -> Response:
    await service.write_pin(body)
    return Response(status_code=204)


@router.get(
    "/lora-parameters",
    response_model=LoRaParameters,
    operation_id="read_transmitter_lora_parameters",
)
async def read_lora(service: TransmitterService = Depends(transmitter_service)) -> LoRaParameters:
    return await service.read_lora()


@router.put("/lora-parameters", status_code=204, operation_id="write_transmitter_lora_parameters")
async def write_lora(
    body: LoRaParameters, service: TransmitterService = Depends(transmitter_service)
) -> Response:
    await service.write_lora(body)
    return Response(status_code=204)


@router.post(
    "/lora-parameters/restore-defaults",
    status_code=204,
    operation_id="restore_transmitter_lora_defaults",
)
async def restore_lora(service: TransmitterService = Depends(transmitter_service)) -> Response:
    await service.restore_lora()
    return Response(status_code=204)


@router.get(
    "/gfsk-parameters",
    response_model=GfskParameters,
    operation_id="read_transmitter_gfsk_parameters",
)
async def read_gfsk(service: TransmitterService = Depends(transmitter_service)) -> GfskParameters:
    return await service.read_gfsk()


@router.put("/gfsk-parameters", status_code=204, operation_id="write_transmitter_gfsk_parameters")
async def write_gfsk(
    body: GfskParameters, service: TransmitterService = Depends(transmitter_service)
) -> Response:
    await service.write_gfsk(body)
    return Response(status_code=204)


@router.post(
    "/gfsk-parameters/restore-defaults",
    status_code=204,
    operation_id="restore_transmitter_gfsk_defaults",
)
async def restore_gfsk(service: TransmitterService = Depends(transmitter_service)) -> Response:
    await service.restore_gfsk()
    return Response(status_code=204)
