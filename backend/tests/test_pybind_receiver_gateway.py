from types import SimpleNamespace
from typing import Any

import pytest

from wrs_debugger.errors import ApplicationError
from wrs_debugger.gateway.pybind import PybindWrsGateway


class FakeTransmitter:
    def __init__(self) -> None:
        self.find_called = False
        self.cancel_called = False

    def prepare_binding(self) -> dict[str, Any]:
        return {"device_id": 0xA1B2C3, "kbind": bytes(range(16)), "transaction_id": 42}

    def find_binding(self, context: dict[str, Any]) -> None:
        assert context["transaction_id"] == 42
        self.find_called = True

    def cancel_binding(self, context: dict[str, Any]) -> None:
        assert context["transaction_id"] == 42
        self.cancel_called = True


class FakeReceiver:
    def __init__(self) -> None:
        self.bound = False
        self.timeout_on_bind = False

    def binding_state(self) -> dict[str, Any]:
        return {"bound": self.bound, "device_id": 0xA1B2C3 if self.bound else 0}

    def start_binding(self, device_id: int, kbind: bytes, transaction_id: int) -> None:
        assert (device_id, kbind, transaction_id) == (0xA1B2C3, bytes(range(16)), 42)
        if self.timeout_on_bind:
            raise RuntimeError("RECEIVER_6")
        self.bound = True

    def read_gfsk(self) -> None:
        raise RuntimeError("RECEIVER_15")


def gateway(
    monkeypatch: pytest.MonkeyPatch,
) -> tuple[PybindWrsGateway, FakeTransmitter, FakeReceiver]:
    transmitter = FakeTransmitter()
    receiver = FakeReceiver()
    monkeypatch.setitem(
        __import__("sys").modules,
        "wrs_debugger_native",
        SimpleNamespace(TransmitterClient=lambda: transmitter, ReceiverClient=lambda: receiver),
    )
    return PybindWrsGateway(), transmitter, receiver


def test_factory_binding_uses_same_material_and_verifies_find(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    adapter, transmitter, receiver = gateway(monkeypatch)
    handle = adapter.prepare_transmitter_binding()
    adapter.prepare_receiver_binding(handle)
    adapter.verify_binding(handle)
    assert transmitter.find_called
    assert receiver.bound
    assert handle.kbind is None


def test_missing_driver_capability_is_explicit(monkeypatch: pytest.MonkeyPatch) -> None:
    adapter, _, _ = gateway(monkeypatch)
    with pytest.raises(ApplicationError) as error:
        adapter.read_receiver_gfsk_parameters()
    assert error.value.code == "RECEIVER_UNSUPPORTED"
    assert error.value.status == 501


def test_prepared_receiver_is_not_unsafely_unbound(monkeypatch: pytest.MonkeyPatch) -> None:
    adapter, transmitter, _ = gateway(monkeypatch)
    handle = adapter.prepare_transmitter_binding()
    adapter.prepare_receiver_binding(handle)
    with pytest.raises(ApplicationError) as error:
        adapter.rollback_binding(handle)
    assert error.value.code == "BINDING_ROLLBACK_UNSUPPORTED"
    assert not transmitter.cancel_called


def test_timeout_does_not_trigger_unsafe_cancel(monkeypatch: pytest.MonkeyPatch) -> None:
    adapter, transmitter, receiver = gateway(monkeypatch)
    handle = adapter.prepare_transmitter_binding()
    receiver.timeout_on_bind = True
    with pytest.raises(ApplicationError):
        adapter.prepare_receiver_binding(handle)
    with pytest.raises(ApplicationError) as error:
        adapter.rollback_binding(handle)
    assert error.value.code == "BINDING_ROLLBACK_UNSUPPORTED"
    assert not transmitter.cancel_called
