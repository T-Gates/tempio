from domain.models import SensorReport, Command, CommandEnvelope
from ports.repository import SensorRepository
from ports.message_broker import CommandPublisher


class SensorService:
    def __init__(self, repo: SensorRepository, publisher: CommandPublisher):
        self.repo = repo
        self.publisher = publisher

    def process_report(self, report: SensorReport):
        self.repo.save_report(report)

    async def send_command(self, hub_id: str, commands: list[Command]):
        envelope = CommandEnvelope(hub_id=hub_id, commands=commands)
        await self.publisher.publish_commands(envelope)

    def get_history(self, limit: int = 100):
        return self.repo.get_reports(limit)

    def get_latest_hub(self):
        return self.repo.get_latest_report()

    def get_latest_sensors(self):
        return self.repo.get_latest_by_node()
