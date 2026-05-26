# Phase 4 완료 보고서

**작업일**: 2026-05-26
**범위**: ESP32 WiFi+MQTT 펌웨어 + 서버 헥사고널 리팩토링

---

## 1. 완료된 작업

### 펌웨어 (ESP32 허브)

| 모듈 | 파일 | 내용 |
|------|------|------|
| WiFi Manager | `src/hub/wifi_manager.cpp/h` | NVS 저장/로드, 시리얼 `wifi set SSID PW` 명령, 자동 재연결 |
| MQTT Client | `src/hub/mqtt_client.cpp/h` | PubSubClient 기반, `seonul/{hub_id}/report` publish + `seonul/{hub_id}/commands` subscribe, 순환 명령 큐(8) |
| BLE Buffer | `src/hub/ble_central.cpp/h` | `SensorReport` 구조체 + `ble_get_pending_report()` 플래그 패턴 |
| Main Orchestrator | `src/hub/main.cpp` | WiFi → BLE → MQTT 루프. 센서 데이터 수신 시 즉시 JSON publish |

**설계 결정**: HTTP 피기백 대신 MQTT(Mosquitto) 채택. BLE 콜백은 NimBLE 내부 스레드에서 실행되므로, 플래그+메인루프 패턴으로 스레드 안전성 확보.

### 인프라

| 컴포넌트 | 파일 | 내용 |
|----------|------|------|
| Docker Compose | `infra/docker-compose.yml` | Mosquitto 2 컨테이너 (1883 포트, 볼륨 마운트) |
| Mosquitto 설정 | `infra/mosquitto/config/mosquitto.conf` | anonymous 허용, persistence 활성화 |

### 서버 (FastAPI 헥사고널 리팩토링)

기존 monolithic `main.py` (214줄)를 아래 구조로 분리:

```
server/
├── main.py                          ← Composition Root (29줄)
├── config.py                        ← pydantic-settings 환경설정
├── domain/
│   ├── models.py                    ← Pydantic 모델 5개
│   └── services.py                  ← SensorService
├── ports/
│   ├── message_broker.py            ← CommandPublisher ABC
│   └── repository.py                ← SensorRepository ABC
├── adapters/
│   ├── inbound/
│   │   ├── http_routes.py           ← FastAPI 라우터 (DI)
│   │   └── mqtt_subscriber.py       ← MQTT 구독 어댑터
│   └── outbound/
│       ├── mqtt_publisher.py        ← MQTT 발행 어댑터
│       └── sqlite_repo.py           ← SQLite 저장소
└── templates/
    └── dashboard.html               ← Jinja2 대시보드
```

---

## 2. 코드 리뷰 수정 내역

2회 자동 리뷰 수행. 총 8건 CRITICAL + 6건 WARNING 발견 → 전량 수정.

### CRITICAL 수정

| # | 이슈 | 파일 | 수정 내용 |
|---|------|------|-----------|
| 1 | async/sync 경계 불일치 | `services.py`, `message_broker.py` | `send_command` → async, 포트도 async 선언 |
| 2 | ISP 위반 (MessageBroker가 pub+sub 결합) | `message_broker.py` | `CommandPublisher` ABC로 분리, subscribe는 인바운드 어댑터 자체 처리 |
| 3 | MqttPublisher가 포트 미상속 | `mqtt_publisher.py` | `class MqttPublisher(CommandPublisher)` |
| 4 | sync 콜백이 이벤트루프 블로킹 | `mqtt_subscriber.py` | `asyncio.to_thread(self._callback, report)` |
| 5 | `HubReport.timestamp` 타입 오류 | `models.py` | `datetime = None` → `Optional[datetime] = None` |
| 6 | 템플릿 경로 잘못 해석 | `http_routes.py` | `.parent.parent` → `.parent.parent.parent` |
| 7 | publish 실패 시 무음 드롭 | `mqtt_publisher.py` | 미연결 시 `RuntimeError` raise |
| 8 | API key 타이밍 공격 취약 | `http_routes.py` | `hmac.compare_digest()` 적용 |

### WARNING 수정

| # | 이슈 | 파일 | 수정 내용 |
|---|------|------|-----------|
| 1 | async def 라우트에서 sync DB 호출 | `http_routes.py` | `async def` → `def` (FastAPI 자동 threadpool) |
| 2 | Jinja2 상대경로 | `http_routes.py` | `Path(__file__).resolve()` 기반 절대경로 |
| 3 | SQLite 커넥션 미종료 | `sqlite_repo.py` | `contextmanager` + `finally: conn.close()` |
| 4 | subscriber task 미정리 | `main.py` | `sub_task.cancel()` + `CancelledError` 처리 |
| 5 | 포트 timestamp 타입 불일치 | `repository.py` | `Optional[datetime]`으로 정렬 |
| 6 | SQLite 동시성 문제 | `sqlite_repo.py` | WAL 모드 + timeout=10 |

---

## 3. 남은 작업 (TODO)

| 우선순위 | 항목 | 비고 |
|----------|------|------|
| **높음** | 실기기 테스트 | Windows PC pull → hub flash → BLE+MQTT E2E |
| **높음** | MQTT publisher 재연결 로직 | subscriber처럼 auto-reconnect 추가 필요 |
| 중간 | FastAPI Docker화 | `docker-compose.yml`에 서비스 추가 |
| 중간 | READ 엔드포인트 인증 | 대시보드+API가 현재 무인증 (public) |
| 중간 | `docs/API.md` 업데이트 | HTTP→MQTT 전환 반영 |
| 낮음 | 모듈레벨 인스턴스화 → lifespan DI | 테스트 용이성 개선 |
| 낮음 | history 엔드포인트 limit 파라미터 | 현재 하드코딩 100 |

---

## 4. 커밋 로그

```
1918eb4 chore: .gitignore에 __pycache__, .env, .db 추가
1220952 refactor: 서버 헥사고널 아키텍처 전환 + 코드리뷰 이슈 수정
37b039f feat: WiFi 매니저 모듈 구현 (NVS + 시리얼 입력)
e361627 feat: MQTT 클라이언트 모듈 구현 (PubSubClient)
b952081 feat: BLE 센서 데이터 공유 버퍼(SensorReport) 추가
4558609 docs: Phase 4 MQTT 설계를 SPEC.md에 통합
```

모든 커밋 원격 push 완료. (`origin/master`)
