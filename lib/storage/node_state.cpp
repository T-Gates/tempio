#include "node_state.h"
#include <Preferences.h>

// Preferences를 .cpp에만 두면 헤더를 include하는 쪽이 Preferences에 의존하지 않는다.
static Preferences prefs;

static constexpr uint16_t MIN_INTERVAL = 10;
static constexpr uint16_t MAX_INTERVAL = 3600;
static constexpr uint16_t DEFAULT_INTERVAL = 60;

void NodeState::begin() {
    prefs.begin("tempio", false);
    _nodeId        = prefs.getUChar("node_id", 0);
    _sleepInterval = prefs.getUShort("interval", DEFAULT_INTERVAL);
    if (_sleepInterval < MIN_INTERVAL || _sleepInterval > MAX_INTERVAL) {
        _sleepInterval = DEFAULT_INTERVAL;
    }
    _opened = true;
}

void NodeState::end() {
    if (_opened) {
        prefs.end();
        _opened = false;
    }
}

void NodeState::clear() {
    if (_opened) prefs.clear();
    _nodeId        = 0;
    _sleepInterval = DEFAULT_INTERVAL;
}

void NodeState::setNodeId(uint8_t id) {
    _nodeId = id;
    if (_opened) prefs.putUChar("node_id", id);
}

void NodeState::setSleepInterval(uint16_t sec) {
    if (sec < MIN_INTERVAL) sec = MIN_INTERVAL;
    if (sec > MAX_INTERVAL) sec = MAX_INTERVAL;
    _sleepInterval = sec;
    if (_opened) prefs.putUShort("interval", sec);
}
