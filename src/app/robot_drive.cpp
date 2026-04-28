/*
  OhmniRobot - ESP32 Motor Controller

  Boot flow:
    1. BLE starts for provisioning/config
    2. Check NVS for saved WiFi credentials
       -> found: WIFI_CONNECTING
       -> not found: BLE_PROVISIONING
    3. WiFi connects -> RUNNING
       -> BLE is turned off to free heap for HTTP + MCP
       -> if WiFi is lost, BLE starts again for reprovisioning
       -> HTTP REST server
       -> MCP WebSocket to XiaoZhi

  Reconfiguration:
    -> Serial command CLEAR wipes WiFi NVS and reboots into BLE provisioning.
*/

#include "../config.h"
#include "../control/motor_control.h"
#include "../control/motion.h"
#include "../interface/serial_cmd.h"
#include "../connectivity/ble_provision.h"
#include "../connectivity/wifi_manager.h"
#include "../interface/http_server.h"
#include "../integrations/robot_mcp.h"
#include "../storage/config_nvs.h"
#include "../control/robot_actions.h"
#include "../control/robot_motion_profiles.h"

// State machine
enum AppState {
    INIT,              // On boot: check NVS
    BLE_PROVISIONING,  // No saved credentials - wait for BLE
    WIFI_CONNECTING,   // Non-blocking WiFi connect (timeout -> BLE)
    RUNNING            // WiFi OK: HTTP + MCP
};

static AppState      s_state     = INIT;
static unsigned long s_wifiStart = 0;
static bool          s_mcpReady  = false;
static bool          s_httpReady = false;
static bool          s_mcpWarned = false;
static unsigned long s_mcpStartedAt = 0;
static unsigned long s_mcpRetryAt = 0;

static RobotMCP* robotMcp() {
    static RobotMCP instance(MCP_ENDPOINT);
    return &instance;
}

// BLE is only needed for provisioning/recovery. Re-enable it whenever the app
// needs to accept new WiFi credentials again.
static void ensureBleProvisioningActive() {
    if (!bleIsActive()) {
        bleProvisionSetup();
    }
    bleStartAdvertising();
}

// BLE is deinitialized after WiFi comes up to recover heap for HTTP + WSS MCP.
// The short delay gives the BLE stack time to release memory before MCP starts.
static void disableBleForRuntime() {
    if (!bleIsActive()) return;
    Serial.printf("[APP] Disabling BLE for WiFi runtime. Free heap before BLE deinit: %u bytes\n", ESP.getFreeHeap());
    bleForceStop();
    delay(50);
    Serial.printf("[APP] BLE disabled for runtime. Free heap after  BLE deinit: %u bytes\n", ESP.getFreeHeap());
}

// Reset only the app-side MCP lifecycle flags. The underlying library is
// expected to reconnect internally once begin()/loop() are running again.
static void resetMcpState() {
    s_mcpReady = false;
    s_mcpWarned = false;
    s_mcpStartedAt = 0;
    s_mcpRetryAt = 0;
}

// Centralize transitions into WIFI_CONNECTING so timeout/retry bookkeeping is
// always reset the same way regardless of whether credentials came from NVS,
// BLE, or an auto-reconnect path.
static void enterWifiConnecting() {
    s_wifiStart = millis();
    s_state = WIFI_CONNECTING;
    resetMcpState();
}

// MCP starts only after BLE has been shut down for memory reasons. If the
// connection does not come up within a grace period, schedule a fresh begin()
// attempt instead of waiting forever in a half-started state.
static void startMcpIfNeeded() {
    unsigned long now = millis();

    if (!s_mcpReady) {
        Serial.printf("[APP] Starting MCP with BLE disabled during runtime. Free heap: %u bytes\n", ESP.getFreeHeap());
        robotMcp()->begin();
        s_mcpReady = true;
        s_mcpWarned = false;
        s_mcpStartedAt = now;
        s_mcpRetryAt = 0;
        return;
    }

    // Once connected, clear transient warning/retry flags so later disconnects
    // can be observed and retried cleanly.
    if (robotMcp()->isConnected()) {
        s_mcpWarned = false;
        s_mcpRetryAt = 0;
        return;
    }

    if (!s_mcpWarned && ESP.getFreeHeap() < MCP_MIN_HEAP_WARN) {
        Serial.printf("[APP] MCP not connected yet. Free heap may still be tight: %u bytes\n", ESP.getFreeHeap());
        s_mcpWarned = true;
    }

    if (s_mcpStartedAt > 0 && now - s_mcpStartedAt >= MCP_CONNECT_GRACE_MS && s_mcpRetryAt == 0) {
        Serial.printf("[APP] MCP still not connected after %lu ms. Scheduling retry. Free heap: %u bytes\n",
                      MCP_CONNECT_GRACE_MS, ESP.getFreeHeap());
        s_mcpRetryAt = now + MCP_RETRY_INTERVAL_MS;
    }

    if (s_mcpRetryAt > 0 && now >= s_mcpRetryAt) {
        Serial.printf("[APP] Retrying MCP begin. Free heap: %u bytes\n", ESP.getFreeHeap());
        s_mcpReady = false;
        s_mcpRetryAt = 0;
    }
}

// Exposed to serial_cmd.cpp
const char* getAppStateStr() {
    switch (s_state) {
        case INIT:             return "INIT";
        case BLE_PROVISIONING: return "BLE_PROVISIONING";
        case WIFI_CONNECTING:  return "WIFI_CONNECTING";
        case RUNNING:          return "RUNNING";
        default:               return "UNKNOWN";
    }
}

// WiFi success is the handoff point from provisioning mode into normal runtime.
// Notify the BLE client first, then free BLE memory, then bring up HTTP/MCP.
static void onWiFiConnected() {
    // Notify BLE client with connection info (BLE still active here)
    String info = "CONNECTED:";
    info += wifiGetIP();
    info += "|" + wifiGetSSID();
    info += "|" + String(wifiGetRSSI());
    info += "|" + wifiGetGateway();
    bleUpdateStatus(info);

    Serial.printf("[APP] WiFi connected: %s\n", wifiGetIP().c_str());

    disableBleForRuntime();

    if (!s_httpReady) {
        httpServerSetup();
        s_httpReady = true;
    }

    resetMcpState();
    s_state = RUNNING;
}

// Setup
void setup() {
    Serial.begin(115200);
    motorSetup();
    configNvsLoad();

    // BLE starts immediately for provisioning. It is disabled once WiFi is
    // stable and will be started again automatically if WiFi is lost.
    bleProvisionSetup();

    Serial.println(F("[APP] OhmniRobot boot."));
}

// Loop
void loop() {
    // FOC must run continuously; every higher-level subsystem feeds targets
    // into this fast loop instead of blocking with delays.
    actionLoop();
    motionProfileLoop();
    motor.loopFOC();
    motor1.loopFOC();
    motor.move(applyDeadzone(target_left) * LEFT_MOTOR_SIGN);
    motor1.move(applyDeadzone(target_right) * RIGHT_MOTOR_SIGN);

    // Serial always active in all states
    handleSerial();

    // BLE loop becomes a no-op after bleForceStop() and resumes after re-setup.
    bleProvisionLoop();

    switch (s_state) {

        // ?? Check NVS on boot ?????????????????????????????????????????????????
        case INIT: {
            String ssid, pass;
            if (wifiLoadCredentials(ssid, pass)) {
                Serial.printf("[APP] Saved WiFi: %s ? connecting...\n", ssid.c_str());
                wifiBeginConnect(ssid, pass);
                enterWifiConnecting();
            } else {
                Serial.println(F("[APP] No saved WiFi ? BLE provisioning."));
                ensureBleProvisioningActive();
                bleUpdateStatus("WAITING");
                s_state = BLE_PROVISIONING;
            }
            break;
        }

        // ?? Wait for SSID+Password from BLE ??????????????????????????????????
        case BLE_PROVISIONING: {
            String ssid, pass;
            if (bleConsumeCredentials(ssid, pass)) {
                wifiBeginConnect(ssid, pass);
                enterWifiConnecting();
            }
            break;
        }

        // ?? Wait for WiFi (timeout 15 s -> back to BLE) ???????????????????????
        case WIFI_CONNECTING: {
            if (wifiIsConnected()) {
                onWiFiConnected();
            } else if (millis() - s_wifiStart > WIFI_TIMEOUT_MS) {
                Serial.println(F("[APP] WiFi timeout ? back to BLE provisioning."));
                ensureBleProvisioningActive();
                bleUpdateStatus("WIFI_FAILED");
                s_state = BLE_PROVISIONING;
            }
            break;
        }

        // ?? WiFi OK: HTTP + MCP WebSocket ?????????????????????????????????????
        case RUNNING: {
            startMcpIfNeeded();

            // Auto-reconnect if WiFi drops
            if (!wifiIsConnected()) {
                Serial.println(F("[APP] WiFi lost ? reconnecting..."));
                ensureBleProvisioningActive();
                bleUpdateStatus("WAITING");
                wifiReconnect();
                enterWifiConnecting();
                break;
            }

            httpServerLoop();
            robotMcp()->loop();
            break;
        }
    }
}

