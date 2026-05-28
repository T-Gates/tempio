from abc import ABC, abstractmethod
from typing import Optional

from domain.models import SensorReport, SensorReportRecord, CommandLog


class SensorRepository(ABC):
    @abstractmethod
    def save_report(self, report: SensorReport) -> None: ...

    @abstractmethod
    def get_reports(self, limit: int = 100) -> list[SensorReportRecord]: ...

    @abstractmethod
    def get_latest_report(self) -> Optional[SensorReport]: ...

    @abstractmethod
    def get_latest_by_node(self) -> dict[str, SensorReportRecord]: ...

    @abstractmethod
    def save_command_log(self, log: CommandLog) -> None: ...

    @abstractmethod
    def update_command_ack(self, hub_id: str, cmd_id: int, success: bool) -> None: ...

    @abstractmethod
    def get_command_logs(self, hub_id: str, limit: int = 50) -> list[CommandLog]: ...
