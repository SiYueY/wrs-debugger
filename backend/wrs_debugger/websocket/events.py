from fastapi import WebSocket, WebSocketDisconnect

from ..models.contracts import Event, EventData, EventType


class ConnectionManager:
    def __init__(self) -> None:
        self.connections: set[WebSocket] = set()

    async def connect(self, socket: WebSocket) -> None:
        await socket.accept()
        self.connections.add(socket)

    def disconnect(self, socket: WebSocket) -> None:
        self.connections.discard(socket)

    async def broadcast(self, event_type: EventType, data: EventData) -> None:
        event = Event(type=event_type, data=data)
        for socket in list(self.connections):
            try:
                await socket.send_json(event.model_dump(mode="json"))
            except WebSocketDisconnect:
                self.disconnect(socket)
