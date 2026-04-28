#include "http_server.h"
#include "../control/motion.h"
#include "../config.h"
#include "../control/robot_actions.h"
#include "../control/robot_motion_profiles.h"

#include <WebServer.h>
#include <WiFi.h>
#include <Arduino.h>

static WebServer s_server(HTTP_PORT);

// ── Helpers ──────────────────────────────────────────────────────────────────
// HTTP input is parsed manually because String::toFloat() silently turns many
// invalid values into 0, which is too ambiguous for a control endpoint.
static bool getSpeed(float* outSpeed, float defaultSpd = 20.0f) {
  if (!outSpeed) return false;
  if (!s_server.hasArg("speed")) {
    *outSpeed = defaultSpd;
    return true;
  }

  String raw = s_server.arg("speed");
  raw.trim();
  if (raw.length() == 0) return false;

  char* endPtr = nullptr;
  float value = strtof(raw.c_str(), &endPtr);
  if (endPtr == raw.c_str() || (endPtr && *endPtr != '\0') || !isfinite(value)) {
    return false;
  }

  *outSpeed = value;
  return true;
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

static void stopAutomations() {
  // Manual HTTP control takes precedence over any running scripted behavior.
  // Stop those engines first so they do not overwrite the new targets on the
  // next loop tick.
  actionStop();
  motionProfileStop();
}

static void sendInvalidSpeed() {
  sendJSON(400, "{\"ok\":false,\"error\":\"invalid_speed\"}");
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
  float spd = 20.0f;
  if (!getSpeed(&spd)) return sendInvalidSpeed();
  stopAutomations();
  Forward(spd);
  sendJSON(200, okJSON("forward", target_left, target_right));
}

static void handleBackward() {
  float spd = 20.0f;
  if (!getSpeed(&spd)) return sendInvalidSpeed();
  stopAutomations();
  Backward(spd);
  sendJSON(200, okJSON("backward", target_left, target_right));
}

static void handleLeft() {
  float spd = 20.0f;
  if (!getSpeed(&spd)) return sendInvalidSpeed();
  stopAutomations();
  TurnLeft(spd);
  sendJSON(200, okJSON("left", target_left, target_right));
}

static void handleRight() {
  float spd = 20.0f;
  if (!getSpeed(&spd)) return sendInvalidSpeed();
  stopAutomations();
  TurnRight(spd);
  sendJSON(200, okJSON("right", target_left, target_right));
}

static void handleStop() {
  stopAutomations();
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
  // Keep the runtime HTTP surface intentionally small: direct drive commands
  // plus a status endpoint for the web control page.
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
