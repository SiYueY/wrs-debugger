import asyncio
import logging
from collections.abc import Awaitable, Callable
from datetime import UTC, datetime
from uuid import uuid4

from wrs_debugger.errors import ApplicationError
from wrs_debugger.models.event import EventName
from wrs_debugger.models.operation import (
    Operation,
    OperationFailure,
    OperationStage,
    OperationState,
    OperationType,
)

logger = logging.getLogger(__name__)

Publish = Callable[[EventName, object], Awaitable[None]]
ProgressUpdate = Callable[[OperationStage, float], Awaitable[None]]
Work = Callable[[ProgressUpdate], Awaitable[None]]
OperationMirror = Callable[[Operation | None], None]


class OperationManager:
    def __init__(self, publish: Publish, mirror: OperationMirror | None = None) -> None:
        self._publish = publish
        self._operations: dict[str, Operation] = {}
        self._active_id: str | None = None
        self._lock = asyncio.Lock()
        self._tasks: set[asyncio.Task[None]] = set()
        self._mirror = mirror

    async def start(self, type_: OperationType, work: Work) -> Operation:
        async with self._lock:
            if self._active_id is not None:
                raise ApplicationError(
                    "OPERATION_CONFLICT",
                    409,
                    "Operation conflict",
                    "Another cross-device operation is already running.",
                )
            now = datetime.now(UTC)
            operation = Operation(
                operation_id=uuid4().hex,
                type=type_,
                state=OperationState.pending,
                stage=OperationStage.pending,
                progress=0.0,
                updated_at=now,
            )
            self._operations[operation.operation_id] = operation
            self._active_id = operation.operation_id
            self._record_active(operation)
        await self._publish_operation(operation)
        task = asyncio.create_task(self._run(operation, work))
        self._tasks.add(task)
        task.add_done_callback(self._tasks.discard)
        return operation.model_copy(deep=True)

    def active(self) -> Operation | None:
        return self._copy(self._operations.get(self._active_id)) if self._active_id else None

    def get(self, operation_id: str) -> Operation:
        operation = self._operations.get(operation_id)
        if operation is None:
            raise ApplicationError(
                "RESOURCE_NOT_FOUND",
                404,
                "Resource not found",
                "Operation was not found.",
                {"operation_id": operation_id},
            )
        return operation.model_copy(deep=True)

    async def shutdown(self) -> None:
        tasks = tuple(self._tasks)
        for task in tasks:
            task.cancel()
        if tasks:
            await asyncio.gather(*tasks, return_exceptions=True)

    async def _run(self, operation: Operation, work: Work) -> None:
        operation.state = OperationState.running
        operation.started_at = datetime.now(UTC)
        operation.updated_at = operation.started_at
        await self._publish_operation(operation)
        try:
            await work(lambda stage, progress: self._update(operation, stage, progress))
        except asyncio.CancelledError:
            operation.state = OperationState.interrupted
            operation.error = OperationFailure(
                code="OPERATION_INTERRUPTED",
                detail="Operation interrupted during backend shutdown.",
            )
            raise
        except ApplicationError as error:
            operation.state = OperationState.failed
            operation.error = OperationFailure(
                code=error.code,
                detail=error.detail,
                context=error.context,
            )
        except Exception:
            logger.exception("Unhandled exception in operation %s", operation.operation_id)
            operation.state = OperationState.failed
            operation.error = OperationFailure(
                code="INTERNAL_ERROR",
                detail="An unexpected error occurred while executing the operation.",
            )
        else:
            operation.state = OperationState.succeeded
            operation.stage = OperationStage.completed
            operation.progress = 1.0
        finally:
            operation.updated_at = datetime.now(UTC)
            async with self._lock:
                if self._active_id == operation.operation_id:
                    self._active_id = None
                    self._record_active(None)
            await self._publish_operation(operation)

    async def _update(self, operation: Operation, stage: OperationStage, progress: float) -> None:
        operation.stage = stage
        operation.progress = progress
        operation.updated_at = datetime.now(UTC)
        self._record_active(operation)
        await self._publish_operation(operation)

    async def _publish_operation(self, operation: Operation) -> None:
        await self._publish(EventName.operation_updated, operation.model_dump(mode="json"))

    @staticmethod
    def _copy(operation: Operation | None) -> Operation | None:
        return operation.model_copy(deep=True) if operation is not None else None

    def _record_active(self, operation: Operation | None) -> None:
        if self._mirror is not None:
            self._mirror(operation)
