import asyncio
import json
from datetime import datetime
from typing import Callable

import aiomqtt

from domain.models import HubReport


class MqttSubscriber:

    def __init__(
        self, broker_host: str = "localhost", broker_port: int = 1883
    ) -> None:
        self.broker_host = broker_host
        self.broker_port = broker_port
        self._callback: Callable[[HubReport], None] | None = None

    def on_report(self, callback: Callable[[HubReport], None]) -> None:
        self._callback = callback

    async def run(self) -> None:
        while True:
            try:
                async with aiomqtt.Client(
                    self.broker_host, self.broker_port
                ) as client:
                    await client.subscribe("seonul/+/report")
                    async for message in client.messages:
                        try:
                            parts = str(message.topic).split("/")
                            if len(parts) != 3:
                                continue
                            hub_id = parts[1]

                            data: dict = json.loads(
                                message.payload.decode()
                            )
                            report = HubReport(
                                hub_id=hub_id,
                                timestamp=datetime.now(),
                                **{
                                    k: v
                                    for k, v in data.items()
                                    if k in HubReport.model_fields
                                    and k not in ("hub_id", "timestamp")
                                },
                            )

                            if self._callback:
                                await asyncio.to_thread(
                                    self._callback, report
                                )
                        except Exception as e:
                            print(f"mqtt parse error: {e}")
            except aiomqtt.MqttError as e:
                print(
                    f"mqtt connection lost: {e}, reconnecting in 5s..."
                )
                await asyncio.sleep(5)
