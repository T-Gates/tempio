# tempio

매장 에어컨 온도 최적화 IoT 플랫폼. 센서 데이터를 실시간 수집하고, 서버에서 에어컨을 원격 제어한다.

> AI·SW 마에스트로 17기 — 팀 T-Gates

## 시스템 구성

```
 ┌─ 매장 ──────────────────────────┐        ┌─ 서버 (RPi) ──────────────┐
 │                                 │        │                           │
 │  센서노드(C3) ─┐                │        │  Mosquitto ←── FastAPI    │
 │                ├─ BLE ─ 허브 ──────WSS────→  :1883        :8000      │
 │  IR노드(C3) ──┘   (ESP32)      │        │  :9001 ←── Cloudflare    │
 │                                 │        │               tunnel     │
 └─────────────────────────────────┘        └───────────────────────────┘
```

| 장치 | 칩 | 역할 | 통신 | 전원 |
|------|-----|------|------|------|
| 허브 | ESP32 | WiFi+BLE 게이트웨이 | MQTT over WSS + BLE Central | USB |
| 센서노드 | ESP32-C3 | 온습도·조도 측정 | BLE Peripheral | AA 건전지 |
| IR노드 | ESP32-C3 | 에어컨 IR 제어 | BLE Peripheral | AA 건전지 |
| 서버 | Raspberry Pi 5 | 데이터 저장·대시보드·명령 발행 | MQTT + HTTP | — |

## 레포 구조

```
tempio/
├── firmware/          PlatformIO — ESP32 펌웨어 (hub / sensor / ir)
├── server/            FastAPI — 헥사고날 아키텍처 (Python)
├── infra/             Docker Compose + Mosquitto 설정
└── docs/              스펙, 로드맵, 개발 노트
```

## 빠른 시작

### 펌웨어

```bash
cd firmware
pio run -e hub    -t upload    # 허브
pio run -e sensor -t upload    # 센서노드
pio run -e ir     -t upload    # IR노드
pio device monitor             # 시리얼 모니터
```

허브 WiFi 설정 (시리얼 모니터에서):
```
wifi set <SSID> <PASSWORD>
```

### 서버

```bash
# Docker Compose (권장)
cd infra && docker compose up -d

# 또는 직접 실행
cd server && uvicorn main:app --host 0.0.0.0 --port 8000
```

환경변수 (`server/.env`):
```
TEMPIO_MQTT_BROKER_HOST=localhost
TEMPIO_DB_PATH=tempio.db
TEMPIO_API_KEY=your-secret-key
```

## 데이터 흐름

**센서 → 서버** (상향):
센서노드 → BLE notify → 허브 → MQTT publish (`tempio/{hub_id}/report`) → WSS → Cloudflare → Mosquitto → FastAPI → SQLite

**서버 → 노드** (하향):
API 호출 → MQTT publish (`tempio/{hub_id}/commands`) → 허브 수신 → BLE write → 노드 실행

## 명령 타입

| 명령 | 대상 | 설명 |
|------|------|------|
| `SET_INTERVAL` | 센서노드 | 측정 주기 변경 (10~3600초) |
| `IR_TIMING` | IR노드 | raw IR 타이밍 배열 발사 |
| `RESET_NODE` | 전체 | 원격 리셋 (재부팅/페어링삭제/공장초기화) |

## 문서

- [SPEC.md](docs/SPEC.md) — BLE 프로토콜, 메시지 포맷, MQTT 설계
- [ROADMAP.md](docs/ROADMAP.md) — 개발 로드맵 (Phase 1~9)
- [API.md](docs/API.md) — MQTT 토픽 + HTTP API 명세
- [NOTES.md](docs/NOTES.md) — 개발 중 만난 이슈와 해결법

## 기술 스택

| 영역 | 기술 |
|------|------|
| 펌웨어 | Arduino + ESP-IDF, NimBLE, ESP-IDF esp_mqtt, ArduinoJson |
| 서버 | FastAPI, aiomqtt, SQLite (WAL), Pydantic |
| 인프라 | Docker Compose, Mosquitto, Cloudflare Tunnel |
| 빌드 | PlatformIO (펌웨어), uvicorn (서버) |

## 라이선스

Private — T-Gates
