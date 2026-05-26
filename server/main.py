from fastapi import FastAPI
from fastapi.responses import HTMLResponse
from pydantic import BaseModel
from datetime import datetime
from typing import Optional

app = FastAPI(title="서늘 테스트 서버")

# ── 인메모리 저장소 (DB 없이 테스트용) ──
hub_data = []           # 허브가 보낸 리포트 이력
sensor_node_data = []   # 센서노드 측정값 이력


# ── Pydantic 모델: 허브가 POST로 보내는 JSON 구조 ──

class DeviceInfo(BaseModel):
    """허브에 BLE로 연결된 개별 디바이스(센서노드 or IR노드) 정보"""
    node_id: str
    node_type: str  # "sensor" | "ir"
    battery_voltage: Optional[float] = None
    rssi: Optional[int] = None  # BLE 신호세기

class SensorNodeReading(BaseModel):
    """센서노드 1개의 측정값 (SHT40 온습도 + LDR 조도)"""
    node_id: str
    temperature: float
    humidity: float
    lux: Optional[float] = None

class HubReport(BaseModel):
    """허브가 주기적으로 서버에 보내는 통합 리포트
    - 허브 자체 상태 (WiFi, 힙, 가동시간)
    - 허브 내장 SCD40 센서값 (CO2, 온도, 습도)
    - BLE로 수집한 센서노드 데이터
    """
    wifi_rssi: Optional[int] = None
    free_heap: Optional[int] = None
    uptime_ms: Optional[int] = None
    co2: Optional[int] = None             # SCD40 CO2 (ppm)
    hub_temperature: Optional[float] = None
    hub_humidity: Optional[float] = None
    connected_devices: list[DeviceInfo] = []        # BLE 연결 디바이스 목록
    sensor_readings: list[SensorNodeReading] = []   # 센서노드 측정값 목록


# ── API 엔드포인트 ──

@app.post("/api/hub/{hub_id}/report")
def receive_hub_report(hub_id: str, report: HubReport):
    """허브로부터 센서 데이터 수신 → 인메모리 저장
    응답의 commands 필드로 IR 제어 명령을 내려보낼 수 있음 (미구현)
    """
    ts = datetime.now().isoformat()

    # 허브 상태 + 메타데이터 저장
    hub_entry = {
        "hub_id": hub_id,
        "wifi_rssi": report.wifi_rssi,
        "free_heap": report.free_heap,
        "uptime_ms": report.uptime_ms,
        "co2": report.co2,
        "hub_temperature": report.hub_temperature,
        "hub_humidity": report.hub_humidity,
        "connected_devices": [d.model_dump() for d in report.connected_devices],
        "timestamp": ts,
    }
    hub_data.append(hub_entry)

    # 센서노드 측정값을 개별 레코드로 풀어서 저장
    for sr in report.sensor_readings:
        sensor_node_data.append({
            "hub_id": hub_id,
            "node_id": sr.node_id,
            "temperature": sr.temperature,
            "humidity": sr.humidity,
            "lux": sr.lux,
            "timestamp": ts,
        })

    device_count = len(report.connected_devices)
    sensor_count = len(report.sensor_readings)
    print(f"[{ts}] hub={hub_id} devices={device_count} sensors={sensor_count} co2={report.co2}")

    # commands: 허브에게 내려보낼 명령 리스트 (예: IR 송출)
    return {"status": "ok", "commands": []}

@app.get("/api/hub-history")
def get_hub_history():
    """허브 리포트 최근 100건 조회"""
    return hub_data[-100:]

@app.get("/api/sensor-history")
def get_sensor_history():
    """센서노드 측정값 최근 100건 조회"""
    return sensor_node_data[-100:]

@app.get("/", response_class=HTMLResponse)
def dashboard():
    """실시간 대시보드 (5초 자동 새로고침)
    - 허브 상태 카드: WiFi RSSI, CO2, 연결 기기 수, Free Heap
    - 연결 디바이스 테이블 + 센서노드 최신값 테이블
    - 온도/습도 추이 차트 (Chart.js)
    """
    latest_hub = hub_data[-1] if hub_data else None
    # 센서노드별 최신 측정값만 추출 (같은 node_id면 덮어쓰기 → 마지막 값)
    latest_sensors = {}
    for d in sensor_node_data:
        latest_sensors[d["node_id"]] = d

    if not latest_hub:
        return """<!DOCTYPE html><html><head><meta charset="utf-8"><meta http-equiv="refresh" content="5">
        <title>서늘 대시보드</title>
        <style>body{font-family:sans-serif;background:#0f172a;color:#64748b;display:flex;justify-content:center;align-items:center;height:100vh;font-size:18px;}</style>
        </head><body>데이터 대기 중... ESP32에서 POST를 보내세요</body></html>"""

    devices = latest_hub.get("connected_devices", [])
    device_rows = ""
    for d in devices:
        bat = f"{d.get('battery_voltage', '-')}V" if d.get('battery_voltage') else "-"
        rssi = f"{d.get('rssi', '-')} dBm" if d.get('rssi') else "-"
        badge = "🌡" if d["node_type"] == "sensor" else "📡"
        device_rows += f"<tr><td>{badge} {d['node_id']}</td><td>{d['node_type']}</td><td>{bat}</td><td>{rssi}</td></tr>"

    sensor_rows = ""
    for nid, d in latest_sensors.items():
        lux = f"{d['lux']}" if d.get('lux') is not None else "-"
        ts = d['timestamp'].split('T')[1][:8]
        sensor_rows += f"<tr><td>{nid}</td><td>{d['temperature']}°C</td><td>{d['humidity']}%</td><td>{lux}</td><td>{ts}</td></tr>"

    # 차트 데이터: 최근 30건의 온도·습도·CO2 추이
    recent_sensors = sensor_node_data[-30:]
    temps_js = str([d['temperature'] for d in recent_sensors])
    hums_js = str([d['humidity'] for d in recent_sensors])
    labels_js = str([d['timestamp'].split('T')[1][:8] for d in recent_sensors])

    co2_recent = [d.get('co2') for d in hub_data[-30:] if d.get('co2') is not None]
    co2_labels = [d['timestamp'].split('T')[1][:8] for d in hub_data[-30:] if d.get('co2') is not None]

    return f"""<!DOCTYPE html>
<html>
<head>
    <meta charset="utf-8">
    <meta http-equiv="refresh" content="5">
    <title>서늘 테스트 대시보드</title>
    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
    <style>
        * {{ margin: 0; padding: 0; box-sizing: border-box; }}
        body {{ font-family: -apple-system, sans-serif; background: #0f172a; color: #e2e8f0; padding: 20px; }}
        h1 {{ font-size: 24px; margin-bottom: 20px; color: #38bdf8; }}
        h2 {{ font-size: 16px; margin: 20px 0 10px; color: #94a3b8; }}
        .cards {{ display: flex; gap: 16px; margin-bottom: 24px; flex-wrap: wrap; }}
        .card {{ background: #1e293b; border-radius: 12px; padding: 20px; min-width: 140px; flex: 1; }}
        .card .label {{ font-size: 12px; color: #94a3b8; margin-bottom: 4px; }}
        .card .value {{ font-size: 28px; font-weight: bold; }}
        .hub {{ color: #38bdf8; }}
        .co2 {{ color: #fbbf24; }}
        .temp {{ color: #f97316; }}
        .hum {{ color: #06b6d4; }}
        .rssi {{ color: #a78bfa; }}
        .count {{ color: #34d399; }}
        .section {{ background: #1e293b; border-radius: 12px; padding: 20px; margin-bottom: 16px; }}
        .chart-container {{ background: #1e293b; border-radius: 12px; padding: 20px; margin-bottom: 16px; }}
        table {{ width: 100%; border-collapse: collapse; }}
        th {{ padding: 8px; text-align: left; font-size: 12px; color: #64748b; border-bottom: 1px solid #334155; }}
        td {{ padding: 8px; font-size: 14px; border-bottom: 1px solid #1e293b; }}
        .grid {{ display: grid; grid-template-columns: 1fr 1fr; gap: 16px; }}
        @media (max-width: 768px) {{ .grid {{ grid-template-columns: 1fr; }} }}
    </style>
</head>
<body>
    <h1>서늘 테스트 대시보드</h1>

    <div class="cards">
        <div class="card"><div class="label">허브 WiFi</div><div class="value rssi">{latest_hub.get('wifi_rssi', '-')} dBm</div></div>
        <div class="card"><div class="label">CO2</div><div class="value co2">{latest_hub.get('co2', '-')} ppm</div></div>
        <div class="card"><div class="label">연결 기기</div><div class="value count">{len(devices)}대</div></div>
        <div class="card"><div class="label">센서 수신</div><div class="value count">{len(sensor_node_data)}건</div></div>
        <div class="card"><div class="label">Free Heap</div><div class="value hub">{latest_hub.get('free_heap', '-'):,} B</div></div>
    </div>

    <div class="grid">
        <div class="section">
            <h2>연결된 기기</h2>
            <table>
                <tr><th>ID</th><th>타입</th><th>배터리</th><th>BLE RSSI</th></tr>
                {device_rows if device_rows else "<tr><td colspan='4' style='color:#64748b'>연결된 기기 없음</td></tr>"}
            </table>
        </div>
        <div class="section">
            <h2>센서노드 최신값</h2>
            <table>
                <tr><th>노드</th><th>온도</th><th>습도</th><th>조도</th><th>시간</th></tr>
                {sensor_rows if sensor_rows else "<tr><td colspan='5' style='color:#64748b'>센서 데이터 없음</td></tr>"}
            </table>
        </div>
    </div>

    <div class="chart-container">
        <canvas id="tempChart" height="80"></canvas>
    </div>

    <script>
        new Chart(document.getElementById('tempChart'), {{
            type: 'line',
            data: {{
                labels: {labels_js},
                datasets: [
                    {{ label: '온도 (°C)', data: {temps_js}, borderColor: '#f97316', tension: 0.3, yAxisID: 'y' }},
                    {{ label: '습도 (%)', data: {hums_js}, borderColor: '#06b6d4', tension: 0.3, yAxisID: 'y1' }}
                ]
            }},
            options: {{
                responsive: true,
                scales: {{
                    y: {{ position: 'left', ticks: {{ color: '#f97316' }}, grid: {{ color: '#334155' }} }},
                    y1: {{ position: 'right', ticks: {{ color: '#06b6d4' }}, grid: {{ display: false }} }},
                    x: {{ ticks: {{ color: '#64748b', maxRotation: 45 }}, grid: {{ color: '#334155' }} }}
                }},
                plugins: {{ legend: {{ labels: {{ color: '#e2e8f0' }} }} }}
            }}
        }});
    </script>
</body>
</html>"""
