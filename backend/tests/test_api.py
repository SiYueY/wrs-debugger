import asyncio
from collections.abc import AsyncIterator
from contextlib import asynccontextmanager
from typing import cast

import httpx
import pytest
from pydantic import ValidationError

from wrs_debugger.gateway.mock import MockWrsGateway
from wrs_debugger.main import app
from wrs_debugger.models.device import SerialPortInfo
from wrs_debugger.models.radio import GfskParameters, LoRaParameters
from wrs_debugger.websocket.hub import WebSocketHub

LORA_ZERO_VALUES = {
    "param_flags": 0,
    "tx_power": 0,
    "freq_offset": 0,
    "payload_len": 0,
    "rssi_threshold": 0,
    "heartbeat_interval": 0,
    "heartbeat_loss": 0,
    "bandwidth": 0,
    "spreading_factor": 0,
    "coding_rate": 0,
    "header_type": 0,
    "preamble_len": 0,
    "sync_word": 0,
}
GFSK_ZERO_VALUES = {
    "param_flags": 0,
    "tx_power": 0,
    "freq_offset": 0,
    "payload_len": 0,
    "rssi_threshold": 0,
    "heartbeat_interval": 0,
    "heartbeat_loss": 0,
    "bandwidth": 0,
    "bitrate": 0,
    "freq_deviation": 0,
    "pulse_shaping": 0,
    "preamble_len": 0,
    "sync_word": 0,
}


@asynccontextmanager
async def api_client() -> AsyncIterator[httpx.AsyncClient]:
    async with app.router.lifespan_context(app):
        transport = httpx.ASGITransport(app=app)
        async with httpx.AsyncClient(transport=transport, base_url="http://testserver") as client:
            yield client


def problem(response: httpx.Response, code: str, status: int) -> None:
    assert response.status_code == status
    body = response.json()
    assert body["code"] == code
    assert body["status"] == status
    assert body["type"].startswith("urn:wrs-debugger:error:")
    assert body["request_id"]


async def connect_both(client: httpx.AsyncClient) -> None:
    assert (
        await client.post("/api/v1/transmitter/connect", json={"device": "/dev/ttyACM0"})
    ).status_code == 200
    assert (await client.post("/api/v1/receiver/connect", json={"domain_id": 0})).status_code == 200


@pytest.mark.asyncio
async def test_system_snapshot_and_openapi_are_v1_only() -> None:
    async with api_client() as client:
        assert (await client.get("/api/v1/health")).json()["status"] == "ok"
        snapshot = (await client.get("/api/v1/snapshot")).json()
        assert snapshot["transmitter_connection"]["state"] == "disconnected"
        schema = (await client.get("/openapi.json")).json()
        paths = schema["paths"]
        assert "/api/v1/transmitter/lora-parameters" in paths
        assert "/api/v1/transmitter/gfsk-parameters" in paths
        assert "/api/v1/receiver/lora-parameters" in paths
        assert "/api/v1/receiver/gfsk-parameters" in paths
        assert not any("connection/robot" in path or "connection/box" in path for path in paths)
        assert not any(path in {"/api/v1/box", "/api/v1/receiver/config"} for path in paths)


@pytest.mark.asyncio
async def test_transmitter_serial_enumeration_switching_failure_and_disconnect() -> None:
    async with api_client() as client:
        gateway = cast(MockWrsGateway, app.state.gateway)
        gateway.available_serial_ports = []
        assert (await client.get("/api/v1/transmitter/serial-ports")).json() == {"ports": []}
        problem(
            await client.post("/api/v1/transmitter/connect", json={"device": "/dev/ttyUSB7"}),
            "SERIAL_PORT_NOT_FOUND",
            404,
        )
        gateway.available_serial_ports = [
            SerialPortInfo(device="/dev/ttyUSB0"),
            SerialPortInfo(device="/dev/ttyUSB1"),
        ]
        first = await client.post("/api/v1/transmitter/connect", json={"device": "/dev/ttyUSB0"})
        assert first.json()["state"] == "connected"
        repeated = await client.post("/api/v1/transmitter/connect", json={"device": "/dev/ttyUSB0"})
        assert repeated.json()["connected_at"] == first.json()["connected_at"]
        switched = await client.post("/api/v1/transmitter/connect", json={"device": "/dev/ttyUSB1"})
        assert switched.json()["device"] == "/dev/ttyUSB1"
        gateway.fail_transmitter_connect = True
        problem(
            await client.post("/api/v1/transmitter/connect", json={"device": "/dev/ttyUSB0"}),
            "TRANSMITTER_CONNECTION_FAILED",
            503,
        )
        assert (await client.get("/api/v1/transmitter/connection")).json()["state"] == "failed"
        assert (await client.post("/api/v1/transmitter/disconnect")).json()[
            "state"
        ] == "disconnected"


@pytest.mark.asyncio
async def test_receiver_settings_and_independent_connection() -> None:
    async with api_client() as client:
        assert (await client.put("/api/v1/receiver/settings", json={"domain_id": 12})).json() == {
            "domain_id": 12
        }
        receiver = await client.post("/api/v1/receiver/connect", json={"domain_id": 12})
        assert receiver.json()["state"] == "connected"
        gateway = cast(MockWrsGateway, app.state.gateway)
        assert gateway.receiver_settings.domain_id == 12
        assert gateway.receiver_connection.state == "connected"
        assert (await client.get("/api/v1/transmitter/connection")).json()[
            "state"
        ] == "disconnected"
        assert (await client.post("/api/v1/receiver/disconnect")).json()["state"] == "disconnected"


@pytest.mark.asyncio
async def test_lora_and_gfsk_raw_parameters_restore_and_numeric_limits() -> None:
    async with api_client() as client:
        problem(
            await client.get("/api/v1/transmitter/lora-parameters"),
            "TRANSMITTER_NOT_CONNECTED",
            409,
        )
        await connect_both(client)
        lora = (await client.get("/api/v1/transmitter/lora-parameters")).json()
        assert lora["rssi_threshold"] == 110
        lora["tx_power"] = -32768
        lora["sync_word"] = 65535
        assert (
            await client.put("/api/v1/transmitter/lora-parameters", json=lora)
        ).status_code == 204
        assert (await client.get("/api/v1/transmitter/lora-parameters")).json()[
            "tx_power"
        ] == -32768
        lora["bandwidth"] = 256
        problem(
            await client.put("/api/v1/transmitter/lora-parameters", json=lora),
            "VALIDATION_FAILED",
            422,
        )
        assert (
            await client.post("/api/v1/transmitter/lora-parameters/restore-defaults")
        ).status_code == 204
        gfsk = (await client.get("/api/v1/receiver/gfsk-parameters")).json()
        gfsk["bitrate"] = 4294967295
        gfsk["freq_deviation"] = 4294967295
        assert (await client.put("/api/v1/receiver/gfsk-parameters", json=gfsk)).status_code == 204
        gfsk["bitrate"] = 4294967296
        problem(
            await client.put("/api/v1/receiver/gfsk-parameters", json=gfsk),
            "VALIDATION_FAILED",
            422,
        )
        assert (
            await client.post("/api/v1/receiver/gfsk-parameters/restore-defaults")
        ).status_code == 204


@pytest.mark.parametrize(
    ("model", "field", "base", "minimum", "maximum"),
    [
        (
            LoRaParameters,
            "bandwidth",
            LORA_ZERO_VALUES,
            0,
            255,
        ),
        (LoRaParameters, "sync_word", LORA_ZERO_VALUES, 0, 65535),
        (LoRaParameters, "tx_power", LORA_ZERO_VALUES, -32768, 32767),
        (
            GfskParameters,
            "bitrate",
            GFSK_ZERO_VALUES,
            0,
            4294967295,
        ),
    ],
)
def test_numeric_type_boundaries(
    model: type[LoRaParameters] | type[GfskParameters],
    field: str,
    base: dict[str, int],
    minimum: int,
    maximum: int,
) -> None:
    for value in (minimum, maximum):
        model.model_validate({**base, field: value})
    for value in (minimum - 1, maximum + 1):
        with pytest.raises(ValidationError):
            model.model_validate({**base, field: value})


@pytest.mark.asyncio
async def test_all_device_parameter_resources_support_get_put_restore() -> None:
    async with api_client() as client:
        problem(await client.get("/api/v1/receiver/lora-parameters"), "RECEIVER_NOT_CONNECTED", 409)
        await connect_both(client)
        for path in (
            "/api/v1/transmitter/lora-parameters",
            "/api/v1/transmitter/gfsk-parameters",
            "/api/v1/receiver/lora-parameters",
            "/api/v1/receiver/gfsk-parameters",
        ):
            parameters = (await client.get(path)).json()
            assert (await client.put(path, json=parameters)).status_code == 204
            assert (await client.post(f"{path}/restore-defaults")).status_code == 204


@pytest.mark.asyncio
async def test_info_pin_and_pin_validation() -> None:
    async with api_client() as client:
        await connect_both(client)
        assert (await client.get("/api/v1/transmitter")).json()["device_id"] == "A1B2C3"
        assert (await client.get("/api/v1/receiver")).json()["bound_device_id"] is None
        assert (await client.get("/api/v1/transmitter/pin")).json()["pin"] == "123456"
        assert (
            await client.put("/api/v1/transmitter/pin", json={"pin": "654321"})
        ).status_code == 204
        assert (await client.get("/api/v1/transmitter/pin")).json()["pin"] == "654321"
        problem(
            await client.put("/api/v1/transmitter/pin", json={"pin": "000000"}),
            "PARAMETER_VALIDATION_FAILED",
            422,
        )
        for pin in ("１２３４５６", "12345"):
            problem(
                await client.put("/api/v1/transmitter/pin", json={"pin": pin}),
                "VALIDATION_FAILED",
                422,
            )


@pytest.mark.asyncio
async def test_unexpected_error_is_problem_details() -> None:
    async with app.router.lifespan_context(app):
        gateway = cast(MockWrsGateway, app.state.gateway)

        def broken_list_serial_ports() -> list[SerialPortInfo]:
            raise RuntimeError("unexpected mock failure")

        gateway.list_serial_ports = broken_list_serial_ports  # type: ignore[method-assign]
        transport = httpx.ASGITransport(app=app, raise_app_exceptions=False)
        async with httpx.AsyncClient(transport=transport, base_url="http://testserver") as client:
            problem(await client.get("/api/v1/transmitter/serial-ports"), "INTERNAL_ERROR", 500)


@pytest.mark.asyncio
async def test_cross_device_operations_and_factory_binding_failure() -> None:
    async with api_client() as client:
        problem(
            await client.post("/api/v1/receiver/lora-parameters/sync-from-transmitter"),
            "TRANSMITTER_NOT_CONNECTED",
            409,
        )
        await connect_both(client)
        transmitter_lora = (await client.get("/api/v1/transmitter/lora-parameters")).json()
        transmitter_lora["spreading_factor"] = 255
        assert (
            await client.put("/api/v1/transmitter/lora-parameters", json=transmitter_lora)
        ).status_code == 204
        response = await client.post("/api/v1/receiver/lora-parameters/sync-from-transmitter")
        assert response.status_code == 202
        operation_id = response.json()["operation_id"]
        assert (await client.get("/api/v1/operations/active")).json()["operation"][
            "operation_id"
        ] == operation_id
        gateway = cast(MockWrsGateway, app.state.gateway)
        assert gateway.active_operation is not None
        assert gateway.active_operation.operation_id == operation_id
        problem(
            await client.put("/api/v1/transmitter/lora-parameters", json=transmitter_lora),
            "OPERATION_CONFLICT",
            409,
        )
        await asyncio.sleep(0.05)
        assert (await client.get(f"/api/v1/operations/{operation_id}")).json()[
            "state"
        ] == "succeeded"
        assert gateway.active_operation is None
        assert (await client.get("/api/v1/receiver/lora-parameters")).json()[
            "spreading_factor"
        ] == 255
        assert (
            await client.post("/api/v1/receiver/gfsk-parameters/sync-from-transmitter")
        ).status_code == 202
        await asyncio.sleep(0.05)
        binding = await client.post("/api/v1/receiver/factory-bind")
        assert binding.status_code == 202
        await asyncio.sleep(0.05)
        assert (await client.get(f"/api/v1/operations/{binding.json()['operation_id']}")).json()[
            "state"
        ] == "succeeded"
        assert (await client.get("/api/v1/receiver")).json()["bound_device_id"] == "A1B2C3"
        gateway.receiver_bound_device_id = None
        gateway.fail_binding_receiver = True
        binding = await client.post("/api/v1/receiver/factory-bind")
        assert binding.status_code == 202
        await asyncio.sleep(0.05)
        assert (await client.get(f"/api/v1/operations/{binding.json()['operation_id']}")).json()[
            "state"
        ] == "failed"
        assert (await client.get("/api/v1/receiver")).json()["bound_device_id"] is None
        assert gateway.binding_handle is None


@pytest.mark.asyncio
async def test_websocket_envelope_has_stable_shape() -> None:
    class FakeSocket:
        async def accept(self) -> None:
            return

    hub = WebSocketHub()
    stream_id = await hub.connect(FakeSocket())  # type: ignore[arg-type]
    await hub.publish(
        event="transmitter.connection.changed",  # type: ignore[arg-type]
        data={"state": "connected"},
    )
    event = hub._clients[stream_id].get_nowait()  # noqa: SLF001
    assert event.model_dump(mode="json").keys() == {
        "version",
        "stream_id",
        "sequence",
        "event",
        "timestamp",
        "data",
    }


@pytest.mark.asyncio
async def test_websocket_receives_connection_parameter_and_operation_events() -> None:
    class FakeSocket:
        async def accept(self) -> None:
            return

    async with api_client() as client:
        hub = cast(WebSocketHub, app.state.websocket_hub)
        stream_id = await hub.connect(FakeSocket())  # type: ignore[arg-type]
        queue = hub._clients[stream_id]  # noqa: SLF001
        await client.post("/api/v1/transmitter/connect", json={"device": "/dev/ttyACM0"})
        connection_events = []
        while not queue.empty():
            connection_events.append(queue.get_nowait().event)
        assert "transmitter.connection.changed" in connection_events
        lora = (await client.get("/api/v1/transmitter/lora-parameters")).json()
        await client.put("/api/v1/transmitter/lora-parameters", json=lora)
        assert queue.get_nowait().event == "transmitter.lora_parameters.changed"
        gfsk = (await client.get("/api/v1/transmitter/gfsk-parameters")).json()
        await client.put("/api/v1/transmitter/gfsk-parameters", json=gfsk)
        assert queue.get_nowait().event == "transmitter.gfsk_parameters.changed"
        await client.post("/api/v1/receiver/connect", json={"domain_id": 0})
        receiver_events = []
        while not queue.empty():
            receiver_events.append(queue.get_nowait().event)
        assert "receiver.connection.changed" in receiver_events
        await client.post("/api/v1/receiver/lora-parameters/sync-from-transmitter")
        assert queue.get_nowait().event == "operation.updated"
