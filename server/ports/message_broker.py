from abc import ABC, abstractmethod

from domain.models import CommandEnvelope


class CommandPublisher(ABC):
    @abstractmethod
    async def connect(self) -> None: ...

    @abstractmethod
    async def disconnect(self) -> None: ...

    @abstractmethod
    async def publish_commands(self, envelope: CommandEnvelope) -> None: ...
