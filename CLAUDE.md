# 서늘 (seonul) — 모노레포

매장 에어컨 온도 최적화 IoT 프로젝트 (SOMA 17기).

## 레포 구조
```
seonul/
├── firmware/           ← ESP32 펌웨어 (PlatformIO)
│   ├── platformio.ini  ← env 3개 (hub / sensor / ir)
│   ├── src/
│   │   ├── hub/        ← 허브 (ESP32, WiFi+BLE Central)
│   │   ├── sensor/     ← 센서노드 (ESP32-C3, BLE Peripheral)
│   │   └── ir/         ← IR노드 (ESP32-C3, BLE Peripheral)
│   └── lib/
│       ├── protocol/   ← BLE 메시지 포맷 공유 헤더
│       └── storage/    ← NVS 영속 저장
├── server/             ← FastAPI 서버 (Python)
│   └── main.py
├── docs/               ← 스펙, 로드맵, 개발 노트
│   ├── SPEC.md
│   ├── ROADMAP.md
│   └── NOTES.md
└── CLAUDE.md
```

## 빌드/실행

### 펌웨어
PlatformIO 프로젝트 루트: `firmware/`
```powershell
cd firmware
pio run -e hub    -t upload   # 허브 플래시
pio run -e sensor -t upload   # 센서노드 플래시
pio run -e ir     -t upload   # IR노드 플래시
pio device monitor            # 시리얼 모니터 (115200)
```

### 서버
```bash
cd server
uvicorn main:app --host 0.0.0.0 --port 8000
```
Cloudflare 터널: `api.yuumi.wiki` → localhost:8000 (systemd 서비스로 자동 실행)

## 시스템 아키텍처

```
서버(FastAPI) ←─ WiFi HTTP ─→ 허브(ESP32)
                                   ↕ BLE
                             센서노드(ESP32-C3)
                             IR노드(ESP32-C3)
```

### 허브 (env:hub)
| 항목 | 내용 |
|------|------|
| 보드 | ESP32 (Phase 1), ESP32-S3 (추후 전환 가능) |
| 센서 | SCD40 (CO2 + 온습도, I2C) — 추후 연결 |
| 전원 | USB (콘센트) |
| 통신 | WiFi → 서버 HTTP, BLE → 센서노드·IR노드 |

### 센서노드 (env:sensor)
| 항목 | 내용 |
|------|------|
| 보드 | ESP32-C3 SuperMini |
| 센서 | DHT22 (테스트), SHT40 (프로덕션) + LDR (조도) |
| 전원 | AA 건전지 2개 + MT3608 부스트 컨버터 |
| 통신 | BLE → 허브 |

### IR노드 (env:ir)
| 항목 | 내용 |
|------|------|
| 보드 | ESP32-C3 SuperMini |
| 액추에이터 | IR LED 940nm + 2N2222 트랜지스터 |
| 전원 | AA 건전지 2개 + MT3608 부스트 컨버터 |
| 통신 | BLE ← 허브 |

## 작업 시 주의
- `.cpp` 파일엔 `#include <Arduino.h>` 직접 포함 필요
- PlatformIO는 `firmware/src/` 아래 env별 서브폴더만 빌드 (`build_src_filter` 사용)
- `firmware/lib/protocol/protocol.h` — 세 env 모두에서 공유, BLE 포맷 변경 시 여기만 수정
- NimBLE 2.5.0 주의사항은 `docs/NOTES.md` 참조
