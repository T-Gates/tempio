#pragma once
#include <stdint.h>

// 노드의 영구 상태를 관리하는 클래스.
// NVS(플래시)에 저장되므로 딥슬립·재부팅 후에도 유지된다.
// 센서노드·IR노드 공용.
class NodeState {
public:
    // NVS 열기 + 저장된 값 복원. setup()에서 한 번 호출.
    void begin();

    // NVS 핸들 닫기. 딥슬립 전에 반드시 호출.
    void end();

    // NVS 전체 삭제 (공장 초기화).
    void clear();

    // ── 노드 ID ──
    // 허브가 ASSIGN_ID로 부여. 0 = 미할당 (신규 노드).
    uint8_t nodeId() const { return _nodeId; }
    void setNodeId(uint8_t id);

    // ── 딥슬립 주기 ──
    // 허브가 SET_INTERVAL로 변경 가능. 10~3600초, 기본 60초.
    uint16_t sleepInterval() const { return _sleepInterval; }
    void setSleepInterval(uint16_t sec);

private:
    uint8_t  _nodeId        = 0;
    uint16_t _sleepInterval = 60;
    bool     _opened        = false;
};
