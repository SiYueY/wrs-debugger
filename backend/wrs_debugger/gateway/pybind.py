from wrs_debugger.gateway.base import WrsGateway
from wrs_debugger.services.native_executor import ThreadedNativeExecutor


class PybindWrsGateway:
    """Reserved seam for the future C++ pybind11 adapter.

    The native module is intentionally unavailable until Receiver protocol clients exist.
    """

    def __init__(self) -> None:
        raise RuntimeError("PybindWrsGateway is not available in Backend V1 mock runtime")

    @staticmethod
    def native_executor() -> ThreadedNativeExecutor:
        return ThreadedNativeExecutor()


def pybind_gateway_contract(_: WrsGateway) -> None:
    """Type-check helper documenting that a future adapter implements WrsGateway."""
