import asyncio
from collections.abc import AsyncIterator
from contextlib import asynccontextmanager
from pathlib import Path
from typing import cast

import httpx
import pytest
from pydantic import ValidationError

from wrs_debugger.errors import ApplicationError
from wrs_debugger.gateway.mock import MockWrsGateway
from wrs_debugger.main import create_app
from wrs_debugger.models.connection import (
    ConnectionState,
    ReceiverConnection,
    TransmitterConnection,
)
from wrs_debugger.models.device import SerialPortInfo
from wrs_debugger.models.diagnostic import DiagnosticError
from wrs_debugger.models.event import EventName
from wrs_debugger.models.gfsk import GfskParameters
from wrs_debugger.models.lora import LoRaParameters
from wrs_debugger.services.native_executor import InlineMockExecutor
from wrs_debugger.services.runtime import Runtime
from wrs_debugger.services.transmitter import TransmitterService
from wrs_debugger.settings import SettingsRepository
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
async def api_client(tmp_path: Path) -> AsyncIterator[tuple[httpx.AsyncClient, object]]:
    app = create_app(
        gateway_factory=MockWrsGateway,
        settings_repository=SettingsRepository(tmp_path / "settings.json"),
    )
    async with app.router.lifespan_context(app):
        transport = httpx.ASGITransport(app=app)
        async with httpx.AsyncClient(transport=transport, base_url="http://testserver") as client:
            yield client, app


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
async def test_transmitter_info_refresh_returns_device_id(tmp_path: Path) -> None:
    async with api_client(tmp_path) as (client, _):
        await client.post("/api/v1/transmitter/connect", json={"device": "/dev/ttyACM0"})
        assert (await client.get("/api/v1/transmitter")).json() == {
            "device_id": "A1B2C3",
            "product_code": 1001,
            "version_number": 1,
            "serial_number": 42,
        }


@pytest.mark.asyncio
async def test_system_contract_and_openapi_problem_details(tmp_path: Path) -> None:
    async with api_client(tmp_path) as (client, _):
        health = (await client.get("/api/v1/health")).json()
        assert health["status"] == "ok"
        assert health["ready"] is True
        assert health["phase"] == "ready_for_control"
        electron_health = await client.get("/api/v1/health", headers={"Origin": "null"})
        assert electron_health.headers["access-control-allow-origin"] == "null"
        vite_health = await client.get(
            "/api/v1/health", headers={"Origin": "http://127.0.0.1:5173"}
        )
        assert vite_health.headers["access-control-allow-origin"] == "http://127.0.0.1:5173"
        rejected_origin = await client.get(
            "/api/v1/health", headers={"Origin": "http://localhost:5173"}
        )
        assert "access-control-allow-origin" not in rejected_origin.headers
        info = (await client.get("/api/v1/system/info")).json()
        assert info["backend_version"] == "0.1.0"
        assert info["api_version"] == "v1"
        assert info["event_version"] == "1.0"
        assert info["backend_instance_id"]
        snapshot = (await client.get("/api/v1/snapshot")).json()
        assert snapshot["transmitter"]["connection"]["state"] == "disconnected"
        assert snapshot["receiver"]["connection"]["state"] == "disconnected"

        schema = (await client.get("/openapi.json")).json()
        paths = schema["paths"]
        assert "/api/v1/transmitter/lora-parameters" in paths
        assert "/api/v1/transmitter/gfsk-parameters" in paths
        assert "/api/v1/receiver/lora-parameters" in paths
        assert "/api/v1/receiver/gfsk-parameters" in paths
        assert "/api/v1/receiver/sdo/read" in paths
        assert "/api/v1/receiver/sdo" in paths
        assert "/api/v1/diagnostics/error" in paths
        response_422 = paths["/api/v1/transmitter/connect"]["post"]["responses"]["422"]
        schema_ref = response_422["content"]["application/json"]["schema"]["$ref"]
        assert schema_ref.endswith("/ProblemDetails")
        assert "HTTPValidationError" not in schema["components"]["schemas"]


@pytest.mark.asyncio
async def test_diagnostic_error_read_and_clear(tmp_path: Path) -> None:
    async with api_client(tmp_path) as (client, app):
        assert (await client.get("/api/v1/diagnostics/error")).json() == {
            "code": None,
            "detail": None,
        }
        gateway = cast(MockWrsGateway, app.state.gateway)
        gateway.diagnostic_error = DiagnosticError(code="E_STOP", detail="Emergency stop active")
        assert (await client.get("/api/v1/diagnostics/error")).json() == {
            "code": "E_STOP",
            "detail": "Emergency stop active",
        }
        assert (await client.post("/api/v1/diagnostics/error/clear")).status_code == 204
        assert (await client.get("/api/v1/diagnostics/error")).json()["code"] is None


@pytest.mark.asyncio
async def test_diagnostic_gateway_failure_uses_problem_details(tmp_path: Path) -> None:
    async with api_client(tmp_path) as (client, app):
        gateway = cast(MockWrsGateway, app.state.gateway)

        def fail_read() -> DiagnosticError:
            raise ApplicationError(
                "DIAGNOSTIC_UNAVAILABLE",
                503,
                "Diagnostics unavailable",
                "The device did not return diagnostic status.",
            )

        gateway.read_diagnostic_error = fail_read  # type: ignore[method-assign]
        problem(
            await client.get("/api/v1/diagnostics/error"),
            "DIAGNOSTIC_UNAVAILABLE",
            503,
        )


@pytest.mark.asyncio
async def test_validation_error_is_i18n_safe_and_does_not_echo_input(tmp_path: Path) -> None:
    async with api_client(tmp_path) as (client, _):
        response = await client.put("/api/v1/transmitter/pin", json={"pin": "１２３４５６"})
        problem(response, "VALIDATION_FAILED", 422)
        body = response.json()
        assert body["violations"] == [
            {"field": "pin", "code": "INVALID_FORMAT", "context": {"pattern": "^[0-9]{6}$"}}
        ]
        assert "１２３４５６" not in response.text


@pytest.mark.asyncio
async def test_transmitter_switching_failure_and_disconnect(tmp_path: Path) -> None:
    async with api_client(tmp_path) as (client, app):
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
        disconnected = await client.post("/api/v1/transmitter/disconnect")
        assert disconnected.json()["state"] == "disconnected"


@pytest.mark.asyncio
async def test_unexpected_connect_failure_converges_to_failed(tmp_path: Path) -> None:
    app = create_app(settings_repository=SettingsRepository(tmp_path / "settings.json"))
    async with app.router.lifespan_context(app):
        gateway = cast(MockWrsGateway, app.state.gateway)

        def broken_connect(_: str) -> None:
            raise RuntimeError("boom")

        gateway.connect_transmitter = broken_connect  # type: ignore[method-assign]
        transport = httpx.ASGITransport(app=app, raise_app_exceptions=False)
        async with httpx.AsyncClient(transport=transport, base_url="http://testserver") as client:
            problem(
                await client.post("/api/v1/transmitter/connect", json={"device": "/dev/ttyACM0"}),
                "INTERNAL_ERROR",
                500,
            )
            connection = (await client.get("/api/v1/transmitter/connection")).json()
            assert connection["state"] == "failed"
            assert connection["error"]["code"] == "INTERNAL_ERROR"


@pytest.mark.asyncio
async def test_native_disconnect_error_updates_connection_state(tmp_path: Path) -> None:
    async with api_client(tmp_path) as (client, app):
        await client.post("/api/v1/transmitter/connect", json={"device": "/dev/ttyACM0"})
        gateway = cast(MockWrsGateway, app.state.gateway)

        def disconnected() -> LoRaParameters:
            raise ApplicationError(
                "TRANSMITTER_DISCONNECTED",
                409,
                "Transmitter disconnected",
                "The transmitter disconnected during I/O.",
            )

        gateway.read_transmitter_lora_parameters = disconnected  # type: ignore[method-assign]
        problem(
            await client.get("/api/v1/transmitter/lora-parameters"),
            "TRANSMITTER_DISCONNECTED",
            409,
        )
        connection = (await client.get("/api/v1/transmitter/connection")).json()
        assert connection["state"] == "disconnected"
        assert connection["error"]["code"] == "TRANSMITTER_DISCONNECTED"


@pytest.mark.asyncio
async def test_idle_connection_monitor_detects_transmitter_loss() -> None:
    events: list[tuple[EventName, object]] = []

    async def publish(event: EventName, data: object) -> None:
        events.append((event, data))

    gateway = MockWrsGateway()
    runtime = Runtime(
        gateway=gateway,
        publish=publish,
        transmitter_connection=TransmitterConnection(
            state=ConnectionState.connected, device="/dev/ttyACM0"
        ),
        receiver_connection=ReceiverConnection(state=ConnectionState.disconnected),
        executor=InlineMockExecutor(),
    )

    def disconnected() -> None:
        raise ApplicationError(
            "TRANSMITTER_DISCONNECTED",
            503,
            "Transmitter disconnected",
            "The transmitter disconnected while idle.",
    )

    gateway.probe_transmitter = disconnected  # type: ignore[method-assign]
    with pytest.raises(ApplicationError, match="TRANSMITTER_DISCONNECTED"):
        await TransmitterService(runtime).probe_connection()
    assert runtime.transmitter_connection.state == ConnectionState.disconnected
    assert runtime.transmitter_connection.error is not None
    assert runtime.transmitter_connection.error.code == "TRANSMITTER_DISCONNECTED"
    assert events[-1][0] == EventName.transmitter_connection_changed


@pytest.mark.asyncio
async def test_restore_defaults_does_not_read_back(tmp_path: Path) -> None:
    async with api_client(tmp_path) as (client, app):
        await connect_both(client)
        gateway = cast(MockWrsGateway, app.state.gateway)

        def forbidden_read() -> LoRaParameters:
            raise RuntimeError("restore must not read back")

        gateway.read_transmitter_lora_parameters = forbidden_read  # type: ignore[method-assign]
        assert (
            await client.post("/api/v1/transmitter/lora-parameters/restore-defaults")
        ).status_code == 204


@pytest.mark.asyncio
async def test_receiver_settings_are_isolated_and_atomic(tmp_path: Path) -> None:
    settings_path = tmp_path / "settings.json"
    async with api_client(tmp_path) as (client, _):
        assert (await client.put("/api/v1/receiver/settings", json={"domain_id": 12})).json() == {
            "domain_id": 12
        }
    assert settings_path.exists()
    assert settings_path.read_text(encoding="utf-8") == '{"receiver": {"domain_id": 12}}'


@pytest.mark.asyncio
async def test_receiver_settings_write_failure_is_reported(tmp_path: Path) -> None:
    class FailingRepository(SettingsRepository):
        def save_domain_id(self, domain_id: int) -> None:
            del domain_id
            raise OSError("read only")

    app = create_app(settings_repository=FailingRepository(tmp_path / "settings.json"))
    async with app.router.lifespan_context(app):
        transport = httpx.ASGITransport(app=app)
        async with httpx.AsyncClient(transport=transport, base_url="http://testserver") as client:
            problem(
                await client.put("/api/v1/receiver/settings", json={"domain_id": 12}),
                "SETTINGS_WRITE_FAILED",
                500,
            )
            assert (await client.get("/api/v1/receiver/settings")).json() == {"domain_id": 0}


@pytest.mark.parametrize(
    ("model", "field", "base", "minimum", "maximum"),
    [
        (LoRaParameters, "bandwidth", LORA_ZERO_VALUES, 0, 255),
        (LoRaParameters, "sync_word", LORA_ZERO_VALUES, 0, 65535),
        (LoRaParameters, "tx_power", LORA_ZERO_VALUES, -32768, 32767),
        (GfskParameters, "bitrate", GFSK_ZERO_VALUES, 0, 4294967295),
        (GfskParameters, "freq_deviation", GFSK_ZERO_VALUES, 0, 4294967295),
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
async def test_receiver_info_does_not_duplicate_connection_state(tmp_path: Path) -> None:
    async with api_client(tmp_path) as (client, _):
        await client.post("/api/v1/receiver/connect", json={"domain_id": 0})
        body = (await client.get("/api/v1/receiver")).json()
        assert body == {"bound_device_id": None}
        assert "connection_state" not in body


@pytest.mark.asyncio
async def test_receiver_sdo_dictionary_permissions_and_response(tmp_path: Path) -> None:
    async with api_client(tmp_path) as (client, app):
        not_connected = await client.post(
            "/api/v1/receiver/sdo/read", json={"object_address": 0x001}
        )
        problem(not_connected, "RECEIVER_NOT_CONNECTED", 409)

        await client.post("/api/v1/receiver/connect", json={"domain_id": 0})
        product = await client.post(
            "/api/v1/receiver/sdo/read", json={"object_address": 0x001}
        )
        assert product.json() == {
            "object_address": 0x001,
            "object_data": 2001,
            "status": 0x4,
            "result_code": 0,
        }
        assert (
            await client.post("/api/v1/receiver/sdo/read", json={"object_address": 0x201})
        ).status_code == 200
        problem(
            await client.post("/api/v1/receiver/sdo/read", json={"object_address": 0x004}),
            "RECEIVER_SDO_INVALID_ADDRESS",
            422,
        )
        problem(
            await client.put(
                "/api/v1/receiver/sdo", json={"object_address": 0x001, "object_data": 0x454E}
            ),
            "RECEIVER_SDO_READ_ONLY",
            422,
        )
        problem(
            await client.put(
                "/api/v1/receiver/sdo", json={"object_address": 0x202, "object_data": 1}
            ),
            "RECEIVER_SDO_INVALID_VALUE",
            422,
        )
        written = await client.put(
            "/api/v1/receiver/sdo",
            json={"object_address": 0x202, "object_data": 0x0000454E},
        )
        assert written.json() == {
            "object_address": 0x202,
            "object_data": 0,
            "status": 0x6,
            "result_code": 0,
        }
        gateway = cast(MockWrsGateway, app.state.gateway)
        assert gateway.receiver_sdo[0x202] == 0x0000454E


@pytest.mark.asyncio
async def test_receiver_rejects_protocol_invalid_parameters(tmp_path: Path) -> None:
    async with api_client(tmp_path) as (client, _):
        await client.post("/api/v1/receiver/connect", json={"domain_id": 0})
        invalid_lora = {
            **LORA_ZERO_VALUES,
            "param_flags": 0,
            "tx_power": 10,
            "payload_len": 12,
            "rssi_threshold": 110,
            "heartbeat_interval": 200,
            "heartbeat_loss": 3,
            "bandwidth": 1,
            "spreading_factor": 6,
            "coding_rate": 4,
            "preamble_len": 11,
            "sync_word": 0x1424,
        }
        response = await client.put("/api/v1/receiver/lora-parameters", json=invalid_lora)
        problem(response, "PARAMETER_VALIDATION_FAILED", 422)
        assert response.json()["context"] == {"field": "preamble_len"}


@pytest.mark.asyncio
async def test_receiver_rejects_invalid_gateway_response(tmp_path: Path) -> None:
    async with api_client(tmp_path) as (client, app):
        await client.post("/api/v1/receiver/connect", json={"domain_id": 0})
        gateway = cast(MockWrsGateway, app.state.gateway)
        gateway.receiver_lora_parameters.preamble_len = 11
        response = await client.get("/api/v1/receiver/lora-parameters")
        problem(response, "RECEIVER_INVALID_RESPONSE", 502)


@pytest.mark.asyncio
async def test_cross_device_operations_use_structured_failure(tmp_path: Path) -> None:
    async with api_client(tmp_path) as (client, app):
        await connect_both(client)
        sync = await client.post("/api/v1/receiver/lora-parameters/sync-from-transmitter")
        assert sync.status_code == 202
        operation_id = sync.json()["operation_id"]
        await asyncio.sleep(0.02)
        operation = (await client.get(f"/api/v1/operations/{operation_id}")).json()
        assert operation["state"] == "succeeded"
        assert operation["stage"] == "completed"
        assert operation["error"] is None

        gateway = cast(MockWrsGateway, app.state.gateway)
        gateway.fail_binding_receiver = True
        binding = await client.post("/api/v1/receiver/factory-bind")
        await asyncio.sleep(0.02)
        failed = (await client.get(f"/api/v1/operations/{binding.json()['operation_id']}")).json()
        assert failed["state"] == "failed"
        assert failed["error"]["code"] == "BINDING_FAILED"
        assert isinstance(failed["error"]["detail"], str)


class FakeSocket:
    def __init__(self) -> None:
        self.closed: tuple[int, str] | None = None
        self.accepted = False
        self.sent: list[object] = []

    async def accept(self) -> None:
        self.accepted = True

    async def close(self, code: int = 1000, reason: str = "") -> None:
        self.closed = (code, reason)

    async def send_json(self, data: object) -> None:
        self.sent.append(data)

    async def receive(self) -> dict[str, object]:
        return {"type": "websocket.disconnect", "code": 1000}


@pytest.mark.asyncio
async def test_websocket_slow_client_is_closed_and_removed() -> None:
    socket = FakeSocket()
    hub = WebSocketHub(queue_size=1)
    stream_id = await hub.connect(cast(object, socket))  # type: ignore[arg-type]
    data = {"state": "connected", "device": "/dev/ttyUSB0", "connected_at": None, "error": None}
    await hub.publish(event=EventName.transmitter_connection_changed, data=data)
    await hub.publish(event=EventName.transmitter_connection_changed, data=data)
    assert stream_id not in hub._clients
    assert socket.closed is not None
    assert socket.closed[0] == 1013


@pytest.mark.asyncio
async def test_websocket_serve_handles_disconnect_frame() -> None:
    socket = FakeSocket()
    hub = WebSocketHub()
    await hub.serve(cast(object, socket))  # type: ignore[arg-type]
    assert socket.accepted
    assert hub._clients == {}
