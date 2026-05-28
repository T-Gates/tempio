import json
import threading
from datetime import datetime

from domain.models import SensorReport, Command, CommandEnvelope, CommandLog
from ports.repository import SensorRepository
from ports.message_broker import CommandPublisher


class SensorService:
    def __init__(self, repo: SensorRepository, publisher: CommandPublisher):
        self.repo = repo
        self.publisher = publisher
        self._cmd_id_counter = 0
        self._cmd_id_lock = threading.Lock()

    def _next_cmd_id(self) -> int:
        with self._cmd_id_lock:
            self._cmd_id_counter = (self._cmd_id_counter % 65535) + 1
            return self._cmd_id_counter

    def process_report(self, report: SensorReport):
        self.repo.save_report(report)

    async def send_command(self, hub_id: str, commands: list[Command]):
        for cmd in commands:
            if cmd.cmd_id == 0:
                cmd.cmd_id = self._next_cmd_id()
        envelope = CommandEnvelope(hub_id=hub_id, commands=commands)
        await self.publisher.publish_commands(envelope)

        now = datetime.now().isoformat()
        for cmd in commands:
            self.repo.save_command_log(CommandLog(
                cmd_id=cmd.cmd_id, hub_id=hub_id, target=cmd.target,
                type=cmd.type, payload=json.dumps(cmd.payload),
                created_at=now,
            ))

    def process_ack(self, hub_id: str, cmd_id: int, success: bool):
        self.repo.update_command_ack(hub_id, cmd_id, success)

    def get_command_logs(self, hub_id: str, limit: int = 50):
        return self.repo.get_command_logs(hub_id, limit)

    def get_history(self, limit: int = 100):
        return self.repo.get_reports(limit)

    def get_latest_hub(self):
        return self.repo.get_latest_report()

    def get_latest_sensors(self):
        return self.repo.get_latest_by_node()
