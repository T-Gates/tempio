# tempio API 명세 (MQTT + HTTP)

Base URL: `https://api.yuumi.wiki`
MQTT Broker: `wss://mqtt.yuumi.wiki/mqtt`

---

## MQTT 토픽

| 방향 | 토픽 패턴 | QoS | 설명 |
|------|-----------|-----|------|
| 허브→서버 | `tempio/{hub_id}/report` | 0 | 센서 데이터 리포트 |
| 서버→허브 | `tempio/{hub_id}/commands` | 0 | 노드 제어 명령 |

- `hub_id` = WiFi MAC, 콜론 제거 소문자 (예: `04b24793c230`)
- 서버는 `tempio/+/report`를 구독하여 자동 처리

---

## 인프라

| 구성요소 | 주소 | 비고 |
|----------|------|------|
| Mosquitto 브로커 (TCP) | `localhost:1883` | 로컬 전용 |
| Mosquitto 브로커 (WebSocket) | `localhost:9001` | 외부 연결용 |
| Cloudflare 터널 (MQTT) | `mqtt.yuumi.wiki` → `localhost:9001` | ESP32 WSS 접속 |
| Cloudflare 터널 (HTTP) | `api.yuumi.wiki` → `localhost:8000` | 대시보드·API |

ESP32 허브는 WSS로 연결: `wss://mqtt.yuumi.wiki/mqtt`

---

## 허브 → 서버 (MQTT)

### 토픽: `tempio/{hub_id}/report`

허브가 주기적(1분)으로 센서 데이터를 MQTT로 퍼블리시. 서버가 구독하여 자동 수신·저장.

**Payload (JSON)**

```json
{
  "wifi_rssi": -45,
  "free_heap": 180000,
  "uptime_ms": 360000,
  "co2": null,
  "hub_temperature": null,
  "hub_humidity": null,
  "connected_devices": [
    {
      "node_id": "aa:bb:cc:dd:ee:01",
      "node_type": "sensor",
      "battery_voltage": 2.85,
      "rssi": -62
    }
  ],
  "sensor_readings": [
    {
      "node_id": "aa:bb:cc:dd:ee:01",
      "temperature": 26.3,
      "humidity": 55.2,
      "lux": null
    }
  ]
}
```

| 필드 | 타입 | 필수 | 설명 |
|------|------|------|------|
| `wifi_rssi` | int? | N | WiFi 신호세기 (dBm) |
| `free_heap` | int? | N | ESP32 남은 힙 (바이트) |
| `uptime_ms` | int? | N | 부팅 후 경과시간 (ms) |
| `co2` | int? | N | SCD40 CO2 (ppm), Phase 6 |
| `hub_temperature` | float? | N | SCD40 온도 (°C), Phase 6 |
| `hub_humidity` | float? | N | SCD40 습도 (%), Phase 6 |
| `connected_devices` | DeviceInfo[] | N | BLE 연결 디바이스 목록 |
| `sensor_readings` | SensorReading[] | N | 센서노드 측정값 목록 |

**DeviceInfo**

| 필드 | 타입 | 필수 | 설명 |
|------|------|------|------|
| `node_id` | string | Y | 노드 BLE MAC 주소 |
| `node_type` | string | Y | `"sensor"` 또는 `"ir"` |
| `battery_voltage` | float? | N | 배터리 전압 (V) |
| `rssi` | int? | N | BLE RSSI (dBm) |

**SensorReading**

| 필드 | 타입 | 필수 | 설명 |
|------|------|------|------|
| `node_id` | string | Y | 센서노드 BLE MAC 주소 |
| `temperature` | float | Y | 온도 (°C) |
| `humidity` | float | Y | 습도 (%) |
| `lux` | float? | N | 조도 (lux) |

---

## 서버 → 허브 (HTTP → MQTT)

### `POST /api/hub/{hub_id}/command`

서버가 허브에 즉시 명령을 전달. HTTP로 요청을 받아 MQTT 토픽 `tempio/{hub_id}/commands`로 퍼블리시.

**Path Parameters**

| 이름 | 타입 | 설명 |
|------|------|------|
| `hub_id` | string | 허브 식별자 (WiFi MAC, 콜론 제거 소문자) |

**Headers**

| 이름 | 필수 | 설명 |
|------|------|------|
| `X-API-Key` | Y* | API 키 (`TEMPIO_API_KEY` 미설정 시 인증 비활성화) |

**Request Body**

```json
{
  "target": "aa:bb:cc:dd:ee:02",
  "type": "SET_INTERVAL",
  "payload": { "interval_sec": 1800 }
}
```

**Command 타입**

| type | target | payload | 설명 |
|------|--------|---------|------|
| `SET_INTERVAL` | 센서노드 MAC | `{ "interval_sec": int }` | 측정 주기 변경 (10~3600) |
| `IR_TIMING` | IR노드 MAC | `{ "timings": int[] }` | IR raw 타이밍 (μs) 발사 |
| `RESET_NODE` | 노드 MAC | `{ "level": int }` | 0=재부팅, 1=페어링삭제, 2=공장초기화 |

**Response `200 OK`**

```json
{
  "status": "ok",
  "detail": "Command published to tempio/04b24793c230/commands"
}
```

---

## 조회 (대시보드·디버그용) — HTTP

### `GET /`

실시간 대시보드 (HTML, Jinja2). 5초 자동 새로고침, Chart.js 온습도 추이 차트.

### `GET /api/hub-history`

허브 리포트 최근 100건.

**Response**: `HubReport[]` (timestamp 포함)

### `GET /api/sensor-history`

센서노드 측정값 최근 100건.

**Response**: `SensorReading[]` (hub_id, timestamp 포함)

---

## 인증

API 키 방식 (`X-API-Key` 헤더). 서버는 `hmac.compare_digest`로 검증.

- 환경변수 `TEMPIO_API_KEY`로 키 설정
- `TEMPIO_API_KEY` 미설정 시 인증 비활성화 (개발 모드)
- 허브 펌웨어에는 NVS 또는 하드코딩으로 저장

---

## 예정 기능

| Phase | 내용 | 비고 |
|-------|------|------|
| 6 | CO2 데이터 | 기존 report payload에 포함 (필드 이미 존재) |
| 후기 | `GET /api/stores` | 매장 목록 조회 |
| 후기 | `PUT /api/stores/{store_id}` | 매장 설정 (영업시간 등) |
| 후기 | `POST /api/hub/{hub_id}/ota` | OTA 펌웨어 업데이트 트리거 |
