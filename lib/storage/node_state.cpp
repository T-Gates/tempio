#include "node_state.h"
#include <Preferences.h>

static Preferences prefs;

static constexpr uint16_t MIN_INTERVAL = 10;
static constexpr uint16_t MAX_INTERVAL = 3600;
static constexpr uint16_t DEFAULT_INTERVAL = 60;

void NodeState::begin() {
    prefs.begin("tempio", false);
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
    _sleepInterval = DEFAULT_INTERVAL;
}

void NodeState::setSleepInterval(uint16_t sec) {
    if (sec < MIN_INTERVAL) sec = MIN_INTERVAL;
    if (sec > MAX_INTERVAL) sec = MAX_INTERVAL;
    _sleepInterval = sec;
    if (_opened) prefs.putUShort("interval", sec);
}
