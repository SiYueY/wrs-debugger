import asyncio
from dataclasses import dataclass
from datetime import UTC, datetime
from uuid import uuid4

from fastapi import WebSocket, WebSocketDisconnect
from pydantic import TypeAdapter

from wrs_debugger.models.event import EventName, WebSocketEvent

_EVENT_ADAPTER: TypeAdapter[WebSocketEvent] = TypeAdapter(WebSocketEvent)


@dataclass(slots=True)
class _Client:
    socket: WebSocket
    queue: asyncio.Queue[WebSocketEvent]


class WebSocketHub:
    def __init__(self, queue_size: int = 64) -> None:
        self._queue_size = queue_size
        self._clients: dict[str, _Client] = {}
        self._sequence = 0
        self._lock = asyncio.Lock()

    async def connect(self, socket: WebSocket) -> str:
        await socket.accept()
        stream_id = uuid4().hex
        async with self._lock:
            self._clients[stream_id] = _Client(
                socket=socket,
                queue=asyncio.Queue(maxsize=self._queue_size),
            )
        return stream_id

    async def disconnect(self, stream_id: str) -> None:
        async with self._lock:
            self._clients.pop(stream_id, None)

    async def publish(self, event: EventName, data: object) -> None:
        async with self._lock:
            self._sequence += 1
            sequence = self._sequence
            clients = list(self._clients.items())
        slow_clients: list[tuple[str, WebSocket]] = []
        for stream_id, client in clients:
            item = _EVENT_ADAPTER.validate_python(
                {
                    "stream_id": stream_id,
                    "sequence": sequence,
                    "event": event.value,
                    "timestamp": datetime.now(UTC),
                    "data": data,
                }
            )
            try:
                client.queue.put_nowait(item)
            except asyncio.QueueFull:
                slow_clients.append((stream_id, client.socket))
        for stream_id, socket in slow_clients:
            await self.disconnect(stream_id)
            try:
                await socket.close(code=1013, reason="WebSocket client is too slow")
            except (RuntimeError, WebSocketDisconnect):
                pass

    async def serve(self, socket: WebSocket) -> None:
        stream_id = await self.connect(socket)
        client = self._clients[stream_id]
        sender = asyncio.create_task(self._send_events(socket, client.queue))
        try:
            while True:
                message = await socket.receive()
                if message.get("type") == "websocket.disconnect":
                    break
        except WebSocketDisconnect:
            pass
        finally:
            sender.cancel()
            await asyncio.gather(sender, return_exceptions=True)
            await self.disconnect(stream_id)

    async def _send_events(self, socket: WebSocket, queue: asyncio.Queue[WebSocketEvent]) -> None:
        while True:
            event = await queue.get()
            await socket.send_json(event.model_dump(mode="json"))
