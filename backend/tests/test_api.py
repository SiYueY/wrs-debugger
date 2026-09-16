import asyncio
from collections.abc import AsyncIterator
from contextlib import asynccontextmanager

import httpx
import pytest

from wrs_debugger.main import app
from wrs_debugger.models.contracts import EventType, Operation, OperationState, OperationType
from wrs_debugger.websocket.events import ConnectionManager


@asynccontextmanager
async def api_client() -> AsyncIterator[httpx.AsyncClient]:
    async with app.router.lifespan_context(app):
        transport = httpx.ASGITransport(app=app)
        async with httpx.AsyncClient(transport=transport, base_url="http://testserver") as client:
            yield client


async def connect_all(client: httpx.AsyncClient) -> None:
    assert (
        await client.post(
            "/api/v1/connection/robot/connect", json={"ip": "192.168.1.10", "domain_id": 0}
        )
    ).status_code == 200
    assert (
        await client.post("/api/v1/connection/box/connect", json={"device": "/dev/ttyACM0"})
    ).status_code == 200


@pytest.mark.asyncio
async def test_health_and_connections() -> None:
    async with api_client() as client:
        assert (await client.get("/api/v1/health")).json()["status"] == "ok"
        assert (await client.get("/api/v1/connection/robot")).json()["state"] == "disconnected"
        assert (await client.get("/api/v1/connection/box/serial-devices")).json() == [
            "/dev/ttyACM0",
            "/dev/ttyUSB0",
        ]
        await connect_all(client)
        assert (await client.post("/api/v1/connection/robot/disconnect")).json()[
            "state"
        ] == "disconnected"


@pytest.mark.asyncio
async def test_box_and_receiver_access_and_validation() -> None:
    async with api_client() as client:
        assert (await client.get("/api/v1/box/config")).status_code == 400
        assert (await client.get("/api/v1/receiver/config")).status_code == 400
        assert (
            await client.put("/api/v1/box/pin", json={"pin": "１２３４５６"})
        ).status_code == 422
        await connect_all(client)

        config = (await client.get("/api/v1/box/config")).json()
        config["wireless_estop_enabled"] = False
        assert (await client.put("/api/v1/box/config", json=config)).json()[
            "wireless_estop_enabled"
        ] is False
        assert (await client.get("/api/v1/receiver/config")).status_code == 200

        invalid_gfsk = {
            **config,
            "phy": {
                "modulation": "gfsk",
                "receive_bandwidth_hz": 117300,
                "bit_rate_bps": 500,
                "frequency_deviation_hz": 600,
                "pulse_shaping": "none",
                "preamble_length": 16,
                "sync_word": 5156,
            },
        }
        assert (await client.put("/api/v1/box/config", json=invalid_gfsk)).status_code == 422
        invalid_gfsk["phy"]["bit_rate_bps"] = 150000
        invalid_gfsk["phy"]["frequency_deviation_hz"] = 600
        assert (await client.put("/api/v1/box/config", json=invalid_gfsk)).status_code == 422
        invalid_gfsk["phy"]["frequency_deviation_hz"] = 50000
        invalid_gfsk["phy"]["receive_bandwidth_hz"] = 100000
        assert (await client.put("/api/v1/box/config", json=invalid_gfsk)).status_code == 422


@pytest.mark.asyncio
async def test_operations_require_connections_and_complete() -> None:
    async with api_client() as client:
        assert (await client.post("/api/v1/receiver/sync-from-box")).status_code == 400
        await connect_all(client)
        response = await client.post("/api/v1/receiver/factory-bind")
        assert response.status_code == 202
        operation_id = response.json()["operation_id"]
        await asyncio.sleep(0.5)
        assert (await client.get(f"/api/v1/operations/{operation_id}")).json()[
            "state"
        ] == "succeeded"
        assert (await client.get("/api/v1/operations/unknown")).status_code == 404


@pytest.mark.asyncio
async def test_sync_copies_box_config_and_rejects_operation_conflicts() -> None:
    async with api_client() as client:
        await connect_all(client)
        config = (await client.get("/api/v1/box/config")).json()
        config["phy"] = {
            "modulation": "gfsk",
            "receive_bandwidth_hz": 234300,
            "bit_rate_bps": 50000,
            "frequency_deviation_hz": 25000,
            "pulse_shaping": "bt_0_5",
            "preamble_length": 16,
            "sync_word": 5156,
        }
        assert (await client.put("/api/v1/box/config", json=config)).status_code == 200
        assert (await client.post("/api/v1/receiver/sync-from-box")).status_code == 202
        assert (await client.post("/api/v1/receiver/factory-bind")).status_code == 409
        await asyncio.sleep(0.4)
        assert (await client.get("/api/v1/receiver/config")).json()["phy"]["modulation"] == "gfsk"


@pytest.mark.asyncio
async def test_websocket_event_payload_shape() -> None:
    class FakeSocket:
        def __init__(self) -> None:
            self.messages: list[dict[str, object]] = []

        async def send_json(self, payload: dict[str, object]) -> None:
            self.messages.append(payload)

    manager = ConnectionManager()
    socket = FakeSocket()
    manager.connections.add(socket)  # type: ignore[arg-type]
    operation = Operation(
        id="abc123",
        type=OperationType.factory_bind,
        state=OperationState.running,
        stage="prepare_box",
        progress=25,
        message="Preparing box",
    )
    await manager.broadcast(EventType.operation_updated, operation)
    assert socket.messages == [
        {"type": "operation_updated", "data": operation.model_dump(mode="json")}
    ]
