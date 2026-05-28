from pydantic import BaseModel
from datetime import datetime
from typing import Optional


class SensorReport(BaseModel):
    hub_id: str
    node_id: str
    node_type: str  # "sensor" | "ir"
    temperature: Optional[float] = None
    humidity: Optional[float] = None
    lux: Optional[float] = None
    battery_voltage: Optional[float] = None
    ble_rssi: Optional[int] = None
    wifi_rssi: Optional[int] = None
    free_heap: Optional[int] = None
    uptime_ms: Optional[int] = None
    timestamp: Optional[datetime] = None

    @classmethod
    def from_mqtt_payload(cls, hub_id: str, data: dict) -> "SensorReport":
        return cls(
            hub_id=hub_id,
            timestamp=datetime.now(),
            **{k: v for k, v in data.items() if k in cls.model_fields and k not in ("hub_id", "timestamp")},
        )


class SensorReportRecord(BaseModel):
    id: int
    hub_id: str
    node_id: str
    node_type: str
    temperature: Optional[float] = None
    humidity: Optional[float] = None
    lux: Optional[float] = None
    battery_voltage: Optional[float] = None
    ble_rssi: Optional[int] = None
    wifi_rssi: Optional[int] = None
    free_heap: Optional[int] = None
    uptime_ms: Optional[int] = None
    timestamp: str


class Command(BaseModel):
    target: str       # 노드 MAC 주소
    type: str         # "SET_INTERVAL", "IR_TIMING", "RESET_NODE", "TEST"
    payload: dict = {}
    cmd_id: int = 0   # 서버가 자동 부여 (0이면 send_command에서 채움)


class CommandEnvelope(BaseModel):
    hub_id: str
    commands: list[Command]
