#include "http_server.h"
#include "motion.h"
#include "config.h"

#include <WebServer.h>
#include <WiFi.h>
#include <Arduino.h>

static WebServer s_server(HTTP_PORT);

// ── Helpers ──────────────────────────────────────────────────────────────────
static float getSpeed(float defaultSpd = 20.0f) {
  return s_server.hasArg("speed") ? s_server.arg("speed").toFloat() : defaultSpd;
}

static void addCORSHeaders() {
  s_server.sendHeader("Access-Control-Allow-Origin",  "*");
  s_server.sendHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
  s_server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

static void sendJSON(int code, const String& body) {
  addCORSHeaders();
  s_server.send(code, "application/json", body);
}

static String okJSON(const char* cmd, float leftTarget, float rightTarget) {
  String j = "{\"ok\":true,\"cmd\":\"";
  j += cmd;
  j += "\",\"target_left\":";
  j += leftTarget;
  j += ",\"target_right\":";
  j += rightTarget;
  j += "}";
  return j;
}

// ── Route handlers ────────────────────────────────────────────────────────────
static void handleForward() {
  float spd = getSpeed();
  Forward(spd);
  sendJSON(200, okJSON("forward", target_left, target_right));
}

static void handleBackward() {
  float spd = getSpeed();
  Backward(spd);
  sendJSON(200, okJSON("backward", target_left, target_right));
}

static void handleLeft() {
  float spd = getSpeed();
  TurnLeft(spd);
  sendJSON(200, okJSON("left", target_left, target_right));
}

static void handleRight() {
  float spd = getSpeed();
  TurnRight(spd);
  sendJSON(200, okJSON("right", target_left, target_right));
}

static void handleStop() {
  Stop();
  sendJSON(200, okJSON("stop", 0, 0));
}

static void handleStatus() {
  String j = "{\"ok\":true,\"target_left\":";
  j += target_left;
  j += ",\"target_right\":";
  j += target_right;
  j += ",\"ip\":\"";
  j += WiFi.localIP().toString();
  j += "\"}";
  sendJSON(200, j);
}

static void handleOptions() {
  addCORSHeaders();
  s_server.send(204);
}

static void handleNotFound() {
  if (s_server.method() == HTTP_OPTIONS) { handleOptions(); return; }
  sendJSON(404, "{\"ok\":false,\"error\":\"not found\"}");
}

// ── Public API ────────────────────────────────────────────────────────────────
void httpServerSetup() {
  s_server.on("/forward",  HTTP_GET, handleForward);
  s_server.on("/backward", HTTP_GET, handleBackward);
  s_server.on("/left",     HTTP_GET, handleLeft);
  s_server.on("/right",    HTTP_GET, handleRight);
  s_server.on("/stop",     HTTP_GET, handleStop);
  s_server.on("/status",   HTTP_GET, handleStatus);
  s_server.onNotFound(handleNotFound);
  s_server.begin();
  Serial.print("[HTTP] Server started at http://");
  Serial.print(WiFi.localIP());
  Serial.println("/");
}

void httpServerLoop() {
  s_server.handleClient();
}
