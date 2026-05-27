from abc import ABC, abstractmethod
from typing import Optional

from domain.models import SensorReport, SensorReportRecord


class SensorRepository(ABC):
    @abstractmethod
    def save_report(self, report: SensorReport) -> None: ...

    @abstractmethod
    def get_reports(self, limit: int = 100) -> list[SensorReportRecord]: ...

    @abstractmethod
    def get_latest_report(self) -> Optional[SensorReport]: ...

    @abstractmethod
    def get_latest_by_node(self) -> dict[str, SensorReportRecord]: ...
