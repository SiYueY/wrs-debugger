from wrs_debugger.models.diagnostic import DiagnosticError
from wrs_debugger.services.runtime import Runtime


class DiagnosticsService:
    def __init__(self, runtime: Runtime) -> None:
        self._runtime = runtime

    async def read_error(self) -> DiagnosticError:
        return await self._runtime.call(self._runtime.gateway.read_diagnostic_error)

    async def clear_error(self) -> None:
        await self._runtime.call(self._runtime.gateway.clear_diagnostic_error)
