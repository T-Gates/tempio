from domain.models import HubReport, Command, CommandEnvelope
from ports.repository import SensorRepository
from ports.message_broker import CommandPublisher


class SensorService:
    def __init__(self, repo: SensorRepository, publisher: CommandPublisher):
        self.repo = repo
        self.publisher = publisher

    def process_report(self, report: HubReport):
        self.repo.save_hub_report(report)
        for reading in report.sensor_readings:
            self.repo.save_sensor_reading(report.hub_id, reading, report.timestamp)

    async def send_command(self, hub_id: str, commands: list[Command]):
        envelope = CommandEnvelope(hub_id=hub_id, commands=commands)
        await self.publisher.publish_commands(envelope)

    def get_hub_history(self, limit: int = 100):
        return self.repo.get_hub_reports(limit)

    def get_sensor_history(self, limit: int = 100):
        return self.repo.get_sensor_readings(limit)

    def get_latest_hub(self):
        return self.repo.get_latest_hub_report()

    def get_latest_sensors(self):
        return self.repo.get_latest_sensor_by_node()
