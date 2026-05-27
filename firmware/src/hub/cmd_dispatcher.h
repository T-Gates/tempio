// MQTT 명령 → BLE 패킷 변환·전달 (서버가 보낸 JSON 명령을 노드가 이해하는 바이너리로)
#pragma once
#include "net/mqtt_handler.h"

void dispatch_command(const MqttCommand& cmd);
