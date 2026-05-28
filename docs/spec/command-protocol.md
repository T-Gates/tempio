# 명령 규약 (서버 → 노드)

## 전체 흐름

```
서버 (FastAPI)
  │  MQTT publish: tempio/{hub_id}/commands
  │  JSON: {"commands": [{target, type, payload}, ...]}
  ▼
Mosquitto (RPi, localhost:1883)
  │  Cloudflare tunnel (WSS :9001)
  ▼
허브 (ESP32, mqtt_handler.cpp)
  │  parseCommands() → MqttCommand 링버퍼에 적재
  │  portMUX 크리티컬 섹션 보호 (MQTT 태스크 → Arduino loop 태스크)
  ▼
허브 (main.cpp loop)
  │  mqtt_get_command() → 큐에서 하나씩 꺼냄
  │  dispatch_command() → type별 핸들러 호출
  ▼
허브 (cmd_dispatcher.cpp)
  │  JSON payload → BLE 바이너리 패킷 변환
  │  ble_send_to_node(target, packet, len)
  ▼
노드 (BLE CONFIG 특성 write)
  │  구조체 그대로 수신 → 동작 실행
  ▼
완료
```

## MQTT Command payload

```json
{
  "commands": [
    { "target": "aa:bb:cc:dd:ee:01", "type": "SET_INTERVAL", "payload": { "interval_sec": 1800 } },
    { "target": "aa:bb:cc:dd:ee:02", "type": "IR_SEND", "payload": { "power": true, "temp": 24, "mode": 1, "fan": 0, "swing": false } },
    { "target": "aa:bb:cc:dd:ee:02", "type": "IR_TIMING", "payload": { "timings": [4500, 4500, 560, 1690] } },
    { "target": "aa:bb:cc:dd:ee:01", "type": "RESET_NODE", "payload": { "level": 0 } }
  ]
}
```

## 명령 타입별 상세

### SET_INTERVAL — 센서노드 측정 주기 변경

| 항목 | 값 |
|------|-----|
| type 문자열 | `"SET_INTERVAL"` (서버·펌웨어 정확히 일치) |
| payload | `{"interval_sec": N}` |
| 기본값 | 60 (필드 없으면) |
| 유효 범위 | 10 ~ 3600초 |
| BLE 패킷 | `SetInterval` 구조체 (3바이트): `[0x12, interval_sec(2B LE)]` |
| 대상 | 센서노드 |
| 노드 동작 | NVS에 저장, 다음 딥슬립부터 적용 |

### RESET_NODE — 노드 리셋

| 항목 | 값 |
|------|-----|
| type 문자열 | `"RESET_NODE"` |
| payload | `{"level": N}` |
| 기본값 | 0 (필드 없으면) |
| BLE 패킷 | `ResetNode` 구조체 (2바이트): `[0x13, level]` |
| 대상 | 모든 노드 |
| level 의미 | 0=재부팅, 1=페어링 삭제, 2=공장 초기화 |

### IR_SEND — 에어컨 제어 (기본 경로)

| 항목 | 값 |
|------|-----|
| type 문자열 | `"IR_SEND"` |
| payload | `{"power": true, "temp": 24, "mode": 1, "fan": 0, "swing": false}` |
| BLE 패킷 | `IrSend` 구조체 (8바이트): `[0x03, cmd_id(2B LE), power, temp, mode, fan, swing]` |
| 대상 | IR노드 |
| 노드 동작 | NVS에서 프로토콜 읽기 → IRremoteESP8266 해당 클래스로 상태 설정 → send() |

**payload 필드:**

| 필드 | 타입 | 기본값 | 설명 |
|------|------|--------|------|
| power | bool | 필수 | true=ON, false=OFF. OFF일 때 나머지 필드 무시 |
| temp | uint8 | 24 | 설정 온도 (16~30) |
| mode | uint8 | 1 | 0=자동(auto), 1=냉방(cool), 2=난방(heat), 3=제습(dry), 4=송풍(fan) |
| fan | uint8 | 0 | 0=자동(auto), 1=약풍(low), 2=중풍(med), 3=강풍(high) |
| swing | bool | false | 스윙 ON/OFF |

mode·fan 값은 브랜드 무관 추상 값. IR노드의 AcController 구현체가 브랜드별 상수로 변환.

### IR_TIMING — IR 신호 발사 (raw fallback)

| 항목 | 값 |
|------|-----|
| type 문자열 | `"IR_TIMING"` |
| payload | `{"timings": [9000, 4500, 560, 1690, ...]}` |
| BLE 패킷 | 가변 길이: `[0x02, cmd_id(2B LE), count(2B LE), timing값들(count×2B LE)]` |
| 크기 제한 | 500바이트 (BLE MTU 512 - 여유분). 초과 시 거부 + 시리얼 로그 |
| 대상 | IR노드 |
| timings 의미 | IR LED on/off 교대 마이크로초. mark(on) → space(off) → mark → ... |
| 노드 동작 | 38kHz PWM 캐리어 생성 + 타이밍대로 GPIO 토글 |
| 용도 | IRremoteESP8266 미지원 프로토콜 대비 raw fallback |

### HUB_STATUS — 허브 상태 조회

| 항목 | 값 |
|------|-----|
| type 문자열 | `"HUB_STATUS"` |
| target | 빈 문자열 (허브 자체 명령) |
| payload | 없음 (`{}`) |
| 동작 | 허브가 상태 JSON을 report 토픽에 즉시 publish |

**응답 payload** (report 토픽):

```json
{
  "type": "hub_status",
  "ble_connected": 2,
  "pending_slots": 1,
  "pending_commands": 3,
  "free_heap": 180000,
  "uptime_ms": 360000,
  "mqtt_connected": true
}
```

서버는 report 수신 시 `type == "hub_status"`이면 DB 저장하지 않고 로그만 남긴다.

## 규칙

1. **type 문자열 매칭**: 서버 `constants.py`와 펌웨어 `cmd_dispatcher.cpp`의 문자열이 정확히 일치해야 함. 오타 시 `unknown cmd` 로그 출력 후 무시.
2. **기본값**: payload에 필드가 없으면 ArduinoJson `|` 연산자로 기본값 적용 (파이썬 `dict.get(key, default)`).
3. **한 번에 여러 명령**: `commands` 배열에 여러 명령 가능. 큐(`CMD_QUEUE_MAX=8`)에 순서대로 적재, loop에서 하나씩 처리.
4. **큐 만석**: 큐가 다 차면 새 명령 버림 (오래된 명령 우선 처리).
5. **대상 미연결**: `ble_send_to_node()` 실패 → PendingPool에 보관 (노드당 최대 4개). `flush_all_pending()`이 매 loop에서 재시도. TTL 5분 초과 시 자동 폐기.
6. **분할 메시지**: MQTT 메시지가 버퍼(1024B)를 초과하면 무시 (`data_len != total_data_len` 체크).
