import sqlite3
from contextlib import contextmanager
from datetime import datetime
from typing import Optional

from domain.models import SensorReport, SensorReportRecord
from ports.repository import SensorRepository


class SqliteRepository(SensorRepository):
    def __init__(self, db_path: str = "tempio.db"):
        self.db_path = db_path
        self._init_tables()

    @contextmanager
    def _conn(self):
        conn = sqlite3.connect(self.db_path, timeout=10)
        conn.row_factory = sqlite3.Row
        try:
            yield conn
            conn.commit()
        except Exception:
            conn.rollback()
            raise
        finally:
            conn.close()

    def _init_tables(self):
        with self._conn() as conn:
            conn.execute("PRAGMA journal_mode=WAL")
            conn.execute("""
                CREATE TABLE IF NOT EXISTS sensor_reports (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    hub_id TEXT NOT NULL,
                    node_id TEXT NOT NULL,
                    node_type TEXT NOT NULL,
                    temperature REAL,
                    humidity REAL,
                    lux REAL,
                    battery_voltage REAL,
                    ble_rssi INTEGER,
                    wifi_rssi INTEGER,
                    free_heap INTEGER,
                    uptime_ms INTEGER,
                    timestamp TEXT NOT NULL
                )
            """)
            conn.execute("CREATE INDEX IF NOT EXISTS idx_sr_node ON sensor_reports(node_id, id)")
            conn.execute("CREATE INDEX IF NOT EXISTS idx_sr_hub ON sensor_reports(hub_id)")

    def save_report(self, report: SensorReport) -> None:
        ts = report.timestamp.isoformat() if report.timestamp else datetime.now().isoformat()
        with self._conn() as conn:
            conn.execute(
                """INSERT INTO sensor_reports
                   (hub_id, node_id, node_type, temperature, humidity, lux,
                    battery_voltage, ble_rssi, wifi_rssi, free_heap, uptime_ms, timestamp)
                   VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)""",
                (report.hub_id, report.node_id, report.node_type,
                 report.temperature, report.humidity, report.lux,
                 report.battery_voltage, report.ble_rssi,
                 report.wifi_rssi, report.free_heap, report.uptime_ms, ts),
            )

    def get_reports(self, limit: int = 100) -> list[SensorReportRecord]:
        with self._conn() as conn:
            rows = conn.execute(
                "SELECT * FROM sensor_reports ORDER BY id DESC LIMIT ?", (limit,)
            ).fetchall()
        result = [SensorReportRecord(**dict(row)) for row in rows]
        result.reverse()
        return result

    def get_latest_report(self) -> Optional[SensorReport]:
        with self._conn() as conn:
            row = conn.execute(
                "SELECT * FROM sensor_reports ORDER BY id DESC LIMIT 1"
            ).fetchone()
        if not row:
            return None
        d = dict(row)
        return SensorReport(
            hub_id=d["hub_id"], node_id=d["node_id"], node_type=d["node_type"],
            temperature=d["temperature"], humidity=d["humidity"], lux=d["lux"],
            battery_voltage=d["battery_voltage"], ble_rssi=d["ble_rssi"],
            wifi_rssi=d["wifi_rssi"], free_heap=d["free_heap"],
            uptime_ms=d["uptime_ms"], timestamp=d["timestamp"],
        )

    def get_latest_by_node(self) -> dict[str, SensorReportRecord]:
        with self._conn() as conn:
            rows = conn.execute("""
                SELECT s.* FROM sensor_reports s
                INNER JOIN (
                    SELECT node_id, MAX(id) as max_id
                    FROM sensor_reports GROUP BY node_id
                ) latest ON s.id = latest.max_id
            """).fetchall()
        return {row["node_id"]: SensorReportRecord(**dict(row)) for row in rows}
