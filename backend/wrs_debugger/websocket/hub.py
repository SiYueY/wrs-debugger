import asyncio
from datetime import UTC, datetime
from uuid import uuid4

from fastapi import WebSocket, WebSocketDisconnect

from wrs_debugger.models.event import EventName, WebSocketEvent


class WebSocketHub:
    def __init__(self, queue_size: int = 64) -> None:
        self._queue_size = queue_size
        self._clients: dict[str, asyncio.Queue[WebSocketEvent]] = {}
        self._sequence = 0
        self._lock = asyncio.Lock()

    async def connect(self, socket: WebSocket) -> str:
        await socket.accept()
        stream_id = uuid4().hex
        async with self._lock:
            self._clients[stream_id] = asyncio.Queue(maxsize=self._queue_size)
        return stream_id

    async def disconnect(self, stream_id: str) -> None:
        async with self._lock:
            self._clients.pop(stream_id, None)

    async def publish(self, event: EventName, data: dict[str, object]) -> None:
        async with self._lock:
            self._sequence += 1
            sequence = self._sequence
            queues = list(self._clients.items())
        for stream_id, queue in queues:
            item = WebSocketEvent(
                stream_id=stream_id,
                sequence=sequence,
                event=event,
                timestamp=datetime.now(UTC),
                data=data,
            )
            try:
                queue.put_nowait(item)
            except asyncio.QueueFull:
                await self.disconnect(stream_id)

    async def serve(self, socket: WebSocket) -> None:
        stream_id = await self.connect(socket)
        queue = self._clients[stream_id]
        sender: asyncio.Task[None] | None = None
        try:
            sender = asyncio.create_task(self._send_events(socket, queue))
            while True:
                await socket.receive()
        except WebSocketDisconnect:
            return
        finally:
            if sender is not None:
                sender.cancel()
                await asyncio.gather(sender, return_exceptions=True)
            await self.disconnect(stream_id)

    async def _send_events(self, socket: WebSocket, queue: asyncio.Queue[WebSocketEvent]) -> None:
        while True:
            event = await queue.get()
            await socket.send_json(event.model_dump(mode="json"))
