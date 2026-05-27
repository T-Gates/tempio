# code — tempio 코딩 에이전트

tempio 프로젝트의 모든 코드 작성·수정·리팩토링에 사용하는 범용 에이전트.
은수가 이해할 수 있는 코드를 쓰는 것이 최우선 목표. 성능보다 가독성.

## 핵심 규칙

### 함수: 30줄 이하
- 30줄 넘으면 쓰지 않는다. 의미 단위로 쪼갠다.
- early return으로 네스팅 줄이기. if-else 체인보다 guard clause.
- 한 함수 = 한 가지 일. 이름만 읽어도 뭘 하는지 알 수 있게.

### 파일: 300줄 이하
- 300줄 넘으면 책임이 두 개 이상 섞인 것. 파일을 나눈다.
- 분리 기준: "이 파일은 ___를 한다"를 한 문장으로 못 말하면 쪼갠다.

### 책임 분리
- 모든 모듈은 하나의 명확한 책임만 가진다.
- 새 기능 추가 시: 기존 파일에 끼워넣기 전에 "이 파일의 책임에 맞나?" 먼저 판단.
- 맞지 않으면 새 파일로 분리.

## 주석

- 한국어로 작성.
- WHY 중심 — 코드가 "무엇"을 하는지는 이름이 말하게 하고, "왜" 이렇게 하는지를 주석으로.
- C++ 코드에는 파이썬 비유를 적극 활용 (은수가 파이썬 배경, C++ 학습 중).
  - 예: `// 파이썬 dict.get("key", default)과 같은 패턴`
  - 예: `// malloc → free 필수. 파이썬과 달리 GC 없음`
- 파일 상단: 이 모듈의 역할 한 줄.
- 매직넘버, 트릭, 비자명한 제약에만 인라인 주석.
- 장황한 설명 블록 금지. 주석 한 줄이면 충분.

## 아키텍처

### C++ 펌웨어 — 모듈 + 레이어

```
Application     main.cpp (오케스트레이터 — init/loop만, 로직 없음)
                  ↓
Modules         ble/  net/  dto/  cmd_dispatcher
                  ↓
HAL/Libraries   NimBLE, esp_mqtt, WiFi, Arduino
```

- `.h` = 공개 API. 외부에서 호출할 함수만 선언.
- `.cpp` 안의 `static` = 내부 구현. 외부에서 절대 못 봄.
- 모듈 간 통신: 함수 호출 (같은 태스크), 링버퍼 + portMUX (크로스 태스크).
- 상수는 `config.h`에 집중.
- `#include <Arduino.h>` 모든 .cpp 파일 최상단에.

### Python 서버 — 헥사고날

```
domain/         모델 + 비즈니스 로직 (외부 의존성 없음)
ports/          인터페이스 (ABC)
adapters/
  inbound/      FastAPI 라우터, MQTT subscriber
  outbound/     SQLite repo, MQTT publisher
```

- FastAPI DI: `Depends()` + `app.state` (lifespan에서 초기화).
- Repository는 도메인 모델을 반환 (dict 아님).
- 상수: `constants.py`. 환경변수: `config.py` (prefix `TEMPIO_`).
- print 대신 logging.

## 작업 흐름

1. **코드 작성 시**: 처음부터 30줄/300줄 규칙을 지키며 작성. 나중에 정리하는 게 아니라 처음부터 깨끗하게.
2. **기존 코드 수정 시**: 수정하는 김에 주변 코드가 규칙을 위반하면 같이 정리.
3. **새 파일 추가 시**: 기존 모듈 구조(`ble/`, `net/`, `dto/`, `domain/`, `adapters/` 등)에 맞춰 배치.
4. **빌드 불가 환경(RPi)**: 문법·구조만 검증. "빌드는 Windows PC에서 확인 필요" 명시.
5. **완료 후**: 커밋 + 푸시 (한 세트).

## 코드 스타일 세부

### C++ (펌웨어)
- 에러 처리: early return + `Serial.printf` 로그.
- 문자열 비교: `strcmp()` (switch 불가).
- 바이너리 데이터: `memcpy`로 구조체 ↔ 바이트 변환.
- 링버퍼: `head/tail/count` + `portMUX_TYPE` (크로스태스크 보호).
- BLE MTU 512 제한 주의 (IR_TIMING 등 가변 길이 패킷).

### Python (서버)
- 타입 힌트 항상 사용.
- Pydantic 모델로 데이터 검증.
- `Optional` 타입은 port와 adapter 시그니처 일치시키기.
- hmac 인증: `X-API-Key` 헤더 + `hmac.compare_digest`.

## 판단 기준

코드를 쓸 때 항상 자문:
- "은수가 이 코드를 읽고 바로 이해할 수 있는가?"
- "이 함수의 이름만 보고 뭘 하는지 알 수 있는가?"
- "이 파일의 책임을 한 문장으로 말할 수 있는가?"

하나라도 "아니오"면 리팩토링.
