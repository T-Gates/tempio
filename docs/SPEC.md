# tempio 펌웨어 스펙

## BLE 프로토콜

### UUID 체계

128-bit UUID. 앞 8자리 `4c544d50` = ASCII "LTMP" (tempio).

| UUID | 용도 |
|------|------|
| `4c544d50-0001-...` | 서비스 |
| `4c544d50-0002-...` | DATA 특성 — 센서값(notify), IR 타이밍(write) |
| `4c544d50-0003-...` | CONFIG 특성 — 설정 명령(write only) |

### 메시지 타입

모든 메시지의 첫 바이트가 타입을 구분한다.

| 코드 | 이름 | 방향 | 특성 | 설명 |
|------|------|------|------|------|
| `0x01` | SENSOR_DATA | 센서노드 → 허브 | DATA (notify) | 온습도 + 조도 + 배터리 |
| `0x02` | IR_TIMING | 허브 → IR노드 | DATA (write) | raw IR 타이밍 배열 |
| `0x10` | HUB_READY | 허브 → 노드 | CONFIG | subscribe 완료 신호 ("데이터 보내도 돼") |
| `0x12` | SET_INTERVAL | 허브 → 센서노드 | CONFIG | 측정 주기 변경 (초) |
| `0x13` | RESET_NODE | 허브 → 노드 | CONFIG | 리셋 (레벨별) |
| `0x20` | NODE_INFO | 노드 → 허브 | DATA (notify) | 연결 직후 자기 소개 |

### 데이터 구조체

모든 구조체는 packed, little-endian. 바이트 그대로 BLE 전송.

**SensorData (13바이트)**

| 오프셋 | 크기 | 필드 | 설명 |
|--------|------|------|------|
| 0 | 1 | type | `0x01` |
| 1 | 4 | temp | 온도 (°C), float |
| 5 | 4 | humidity | 습도 (%), float |
| 9 | 2 | ldr | 조도 raw ADC (0~4095) |
| 11 | 2 | battery_mv | 배터리 전압 (mV) |

**NodeInfo (6바이트)**

| 오프셋 | 크기 | 필드 | 설명 |
|--------|------|------|------|
| 0 | 1 | type | `0x20` |
| 1 | 1 | node_type | `0x01`=센서, `0x02`=IR |
| 2 | 2 | battery_mv | 배터리 전압 (mV) |
| 4 | 1 | fw_major | 펌웨어 버전 major |
| 5 | 1 | fw_minor | 펌웨어 버전 minor |

노드 식별은 BLE MAC 주소로 — 별도 ID 부여 불필요.

**IR_TIMING (가변 길이)**

| 오프셋 | 크기 | 필드 | 설명 |
|--------|------|------|------|
| 0 | 1 | type | `0x02` |
| 1 | 2 | count | 타이밍 원소 개수 |
| 3 | count×2 | timings | mark/space 교대 (μs 단위) |

예: 삼성 냉방 26도 → count=199, 총 401바이트.

**HubReady (1바이트)**: `[0x10]` — 허브가 서비스 탐색 + DATA 구독 완료 후 전송. 노드는 이걸 받으면 NODE_INFO를 보내도 안전.
**SetInterval (3바이트)**: `[0x12, interval_sec(2)]` — 기본 60초, 범위 10~3600
**ResetNode (2바이트)**: `[0x13, level]` — 0=재부팅, 1=페어링 삭제, 2=공장 초기화

### MTU

ESP32끼리 통신이므로 MTU 512바이트 협상. IR 타이밍 최대 ~600바이트도 한 번에 전송 가능. 패킷 분할 불필요.

Central(허브) 측에서 `NimBLEDevice::setMTU(512)` 호출.

---

## 노드별 펌웨어 동작

### 허브 (ESP32)

상시 전원(USB). WiFi + BLE central 동시 운영.

```
[부팅]
  WiFi 연결 (NVS에서 SSID/PW 로드, 최대 10초)
  ├── 성공 → MQTT 브로커 연결 (WSS)
  └── 실패 → BLE만 동작, 시리얼에서 wifi set 대기
  BLE 스캔 시작 (WiFi 성공/실패 무관)

[메인 루프]
  BLE 처리 (스캔, 연결, 데이터 수신)
  MQTT WiFi 상태 감지 (연결 시 start, 끊김 시 stop)
  새 센서 데이터 → MQTT publish (즉시)
  서버 명령 수신 → BLE로 해당 노드에 전달
```

BLE 동시 연결: 안정 4~5대 (WiFi 병행). 소규모 매장(에어컨 1~3대 + 센서 1~2대) 충분.

### 센서노드 (ESP32-C3)

AA 건전지 구동. 대부분 딥슬립.

```
[딥슬립] → 1분 타이머 (서버에서 변경 가능)
[wake-up]
  BLE peripheral 광고 (TEMPIO_SERVICE_UUID)
  허브 연결 대기 (5초 타임아웃)
  HUB_READY 대기 (3초 타임아웃)
  NODE_INFO notify 전송 (타입, 배터리, 펌웨어 버전)
  SensorData notify 전송
  BLE 해제 → 딥슬립
```

### IR노드 (ESP32-C3)

AA 건전지 구동. IR 명령 대기.

```
[딥슬립] → 5분 타이머
[wake-up]
  BLE peripheral 광고
  허브 연결 대기
  명령 있음 → IR 타이밍 수신 → 38kHz PWM + GPIO로 IR 발사
  명령 없음 → 배터리 전압만 보고
  BLE 해제 → 딥슬립
```

IR 발사: `ledcWrite`로 38kHz 캐리어 생성, 타이밍 배열대로 on/off 토글.

---

## 연결 흐름

### 최초 연결

1. 노드 전원 ON → BLE peripheral 광고 (TEMPIO_SERVICE_UUID 포함)
2. 허브가 스캔 → TEMPIO_SERVICE_UUID 발견 → pending 큐에 주소 추가
3. loop()에서 큐에서 주소를 꺼내 연결 시도
4. 서비스 탐색 → DATA 특성 구독 (subscribe)
5. 허브가 CONFIG 특성으로 HUB_READY(`0x10`) 전송
6. 노드가 HUB_READY 수신 → NODE_INFO notify 전송 (타입, 배터리, 펌웨어 버전)
7. 노드는 BLE MAC 주소로 식별 — 별도 ID 부여 없음

### 일상 통신

1. 노드 wake-up → BLE 광고 (TEMPIO_SERVICE_UUID)
2. 허브 연결 → MTU 협상(512) → HUB_READY 전송
3. 노드: NODE_INFO → SensorData notify (센서노드), 또는 IR_TIMING 수신 대기 (IR노드)
4. 연결 해제 → 노드 딥슬립

---

## MQTT 통신

### 설계 판단

| 항목 | 결정 | 이유 |
|------|------|------|
| 프로토콜 | MQTT (Mosquitto) | 논블로킹 publish, 양방향 실시간, IoT 표준 |
| 전송 계층 | WSS (WebSocket Secure) | Cloudflare 터널 경유에 필요. TCP 1883은 터널 불가 |
| MQTT 라이브러리 | ESP-IDF esp_mqtt | WSS 네이티브 지원. PubSubClient는 TCP만 지원하여 교체 |
| WiFi 설정 | NVS + 시리얼 입력 | 코드 재플래시 없이 WiFi 변경 가능 |
| 업로드 타이밍 | BLE notify 즉시 | 센서 데이터 도착 즉시 서버 전달 |
| BLE+WiFi 공존 | 공유 버퍼 + 플래그 + 메인루프 패턴 | BLE 콜백에서 직접 publish 안 함 |
| Topic 구조 | 평면 (`tempio/{hub_id}/...`) | 허브 1대 기준. 매장 확장 시 앞에 store_id 추가 |
| QoS | 0 | 현재 구현 기준. 센서 데이터는 주기적이므로 유실 허용 |

### 허브 파일 구조

```
firmware/src/hub/
├── config.h              설정값 (MQTT 브로커 URI, 루프 주기)
├── main.cpp              오케스트레이터 (setup/loop)
├── dto/
│   └── sensor_report.h   BLE·MQTT·main 간 공유 DTO
├── ble/
│   └── ble_central.cpp/h BLE 스캔·연결·수신 (다중 연결)
└── net/
    ├── wifi_manager.cpp/h WiFi 연결 + NVS SSID/PW 관리
    └── mqtt_client.cpp/h  MQTT WSS 클라이언트 (ESP-IDF esp_mqtt)
```

### 데이터 흐름

```
센서노드 ──BLE notify──→ ble_central (onDataNotify)
                              │
                         공유 버퍼 + 플래그
                              │
main.cpp loop() ──────→ mqtt_publish_report()
                              │
                      esp_mqtt → wss://mqtt.yuumi.wiki/mqtt
                              │
                      Cloudflare tunnel → Mosquitto (RPi, :9001)
                              │
                      FastAPI 서버 (aiomqtt, localhost:1883 구독)
```

```
서버 ──MQTT publish──→ Mosquitto (localhost:1883)
                            │
                      Cloudflare tunnel (WSS :9001)
                            │
                      esp_mqtt 콜백 (허브)
                            │
                       명령 큐 저장
                            │
                main.cpp loop()에서 처리
                            │
                     ble_send_to_node()
```

### Topic 구조

| 방향 | topic | QoS |
|------|-------|-----|
| 허브 → 서버 | `tempio/{hub_id}/report` | 0 |
| 서버 → 허브 | `tempio/{hub_id}/commands` | 0 |

hub_id = ESP32 WiFi MAC 주소, 콜론 제거 소문자 (예: `aabbccddeeff`).

### Report payload (허브 → 서버)

```json
{
  "wifi_rssi": -45,
  "free_heap": 180000,
  "uptime_ms": 360000,
  "connected_devices": [
    { "node_id": "aa:bb:cc:dd:ee:01", "node_type": "sensor", "battery_voltage": 2.85, "rssi": -62 }
  ],
  "sensor_readings": [
    { "node_id": "aa:bb:cc:dd:ee:01", "temperature": 26.3, "humidity": 55.2, "lux": null }
  ]
}
```

### Command payload (서버 → 허브)

```json
{
  "commands": [
    { "target": "aa:bb:cc:dd:ee:01", "type": "SET_INTERVAL", "payload": { "interval_sec": 1800 } },
    { "target": "aa:bb:cc:dd:ee:02", "type": "IR_TIMING", "payload": { "timings": [4500, 4500, 560, 1690] } },
    { "target": "aa:bb:cc:dd:ee:01", "type": "RESET_NODE", "payload": { "level": 0 } }
  ]
}
```

### WiFi 설정

시리얼 모니터에서 `wifi set <SSID> <PW>` 입력 → NVS 저장. 재부팅 후에도 유지.

WiFi 없어도 BLE는 동작. WiFi 복구 시 esp_mqtt가 자동으로 start되어 publish 재개.

### 재연결

- WiFi 끊김 → `WiFi.setAutoReconnect(true)` 자동 복구
- MQTT 끊김 → esp_mqtt가 자동 재연결 처리 (내부 FreeRTOS 태스크)
- WiFi 끊긴 동안 BLE 데이터 → 버퍼에 최신 1건만 유지 (덮어쓰기)

### 인프라

- **브로커**: Mosquitto (Docker Compose), 라즈베리파이
  - TCP 1883 (서버 → 브로커, 로컬)
  - WebSocket 9001 (허브 → 브로커, Cloudflare 터널 경유)
- **Cloudflare 터널**: `wss://mqtt.yuumi.wiki/mqtt` → localhost:9001
- **서버**: FastAPI (Docker Compose), aiomqtt로 `tempio/+/report` 구독
- **명령**: 서버 API → MQTT publish → 허브 수신
- **Docker Compose**: Mosquitto + FastAPI 서버를 함께 구동 (`infra/docker-compose.yml`)

---

## 영업시간 기반 절전 (Scheduled Power Saving)

센서노드·IR노드는 건전지 구동이므로, 영업 외 시간에 측정 빈도를 낮춰 배터리 수명을 연장한다.

### 동작 방식

서버가 매장별 영업시간을 관리하고, 허브 리포트 응답(피기백)으로 `SET_INTERVAL` 명령을 내린다.

| 시간대 | 센서노드 주기 | IR노드 주기 | 이유 |
|--------|--------------|------------|------|
| 영업 중 | 1분 (기본) | 5분 | 실시간 모니터링 + 에어컨 제어 |
| 영업 외 | 30분 | 깨우지 않음 | 건물 열특성 데이터 수집 (RL 학습용) |

### 왜 완전 차단이 아닌 저빈도 모니터링인가

에어컨이 꺼진 뒤 매장 온도가 올라가는 속도 = 건물의 열특성.
이 데이터가 있어야 RL 모델이 "에어컨을 몇 시에 미리 켜야 영업 시작 시 쾌적한지" 예측할 수 있다.

### 구현

- **서버**: 매장별 영업시간 설정 (예: 09:00~22:00). 허브 리포트 응답에 `SET_INTERVAL` 포함.
- **허브**: 서버 응답의 `SET_INTERVAL`을 해당 노드에 BLE로 전달 (기존 프로토콜 그대로).
- **노드**: NVS에 interval 저장, 다음 딥슬립부터 적용. 추가 펌웨어 변경 불필요.
- **전환 타이밍**: 영업 시작/종료 시각 직전 리포트에서 interval 변경 명령 발송.

---

## OTA

허브만 해당. 센서노드/IR노드는 BLE OTA 하지 않음.

- **Phase 1 (MVP)**: 수동 USB 플래시
- **Phase 2 (5개 매장+)**: 서버 → WiFi HTTP OTA
  - 허브가 부팅 시(또는 매 N시간) 서버에 현재 버전 전송
  - 서버가 새 버전이면 바이너리 URL 응답
  - 허브가 다운로드 → OTA 파티션에 기록 → 재부팅
  - 실패 시 자동 롤백 (ESP32 기본 기능)

---

## 라이브러리

| 노드 | 라이브러리 | 용도 |
|------|-----------|------|
| 공통 | NimBLE-Arduino | BLE (ESP32 기본보다 가볍고 안정적) |
| 허브 | Sensirion I2C SCD4x | CO2 + 온습도 |
| 허브 | ESP-IDF esp_mqtt | MQTT (WSS 네이티브 지원, Cloudflare 터널 경유) |
| 허브 | ArduinoJson | JSON 직렬화/역직렬화 |
| 센서 | DHT sensor library (테스트) / Sensirion I2C SHT4x (프로덕션) | 온습도 |
| IR | IRremoteESP8266 | IR 프로토콜 (67개 프로토콜, 85+ 브랜드) |
