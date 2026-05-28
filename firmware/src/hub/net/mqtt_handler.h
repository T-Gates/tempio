// MQTT 클라이언트 — ESP-IDF esp_mqtt 기반, WebSocket(WSS) 전송
// Cloudflare 터널 경유: wss://mqtt.yuumi.wiki/mqtt → 로컬 Mosquitto
//
// 수신된 명령은 글로벌 큐 없이 바로 dispatchCommand()로 전달.
// 노드별 펜딩 큐는 cmd_dispatcher.cpp에서 관리.
#pragma once
#include "../dto/mqtt_command.h"

// MQTT 클라이언트 초기화. broker_uri 예: "wss://mqtt.yuumi.wiki/mqtt"
// WiFi 연결되면 자동으로 브로커 접속 시작.
void mqttInit(const char* broker_uri);

// WiFi 연결 상태 변화에 따라 MQTT start/stop 관리. loop()에서 호출.
void mqttLoop();

// 브로커에 연결된 상태인지.
bool mqttIsConnected();

// 센서 리포트 JSON을 tempio/{hub_id}/report 토픽에 발행.
bool mqttPublishReport(const char* json);
