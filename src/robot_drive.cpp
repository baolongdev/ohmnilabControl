/*
  OhmniRobot - ESP32 Motor Controller

  Boot flow:
    1. BLE starts for provisioning/config
    2. Check NVS for saved WiFi credentials
       -> found: WIFI_CONNECTING
       -> not found: BLE_PROVISIONING
    3. WiFi connects -> RUNNING
       -> BLE deinit frees heap needed for SSL
       -> HTTP REST server
       -> MCP WebSocket to XiaoZhi AI

  Reconfiguration:
    -> Serial command CLEAR wipes WiFi NVS and reboots into BLE provisioning.
*/

#include "config.h"
#include "motor_control.h"
#include "motion.h"
#include "serial_cmd.h"
#include "ble_provision.h"
#include "wifi_manager.h"
#include "http_server.h"
#include "robot_mcp.h"
#include "config_nvs.h"
#include "robot_actions.h"
#include "robot_motion_profiles.h"

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

static RobotMCP s_robotMcp(MCP_ENDPOINT);

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

// Called when WiFi successfully connects
static void onWiFiConnected() {
    // Notify BLE client with connection info (BLE still active here)
    String info = "CONNECTED:";
    info += wifiGetIP();
    info += "|" + wifiGetSSID();
    info += "|" + String(wifiGetRSSI());
    info += "|" + wifiGetGateway();
    bleUpdateStatus(info);

    Serial.printf("[APP] WiFi connected: %s\n", wifiGetIP().c_str());

    if (!s_httpReady) {
        httpServerSetup();
        s_httpReady = true;
    }
    s_state = RUNNING;
}

// Setup
void setup() {
    Serial.begin(115200);
    motorSetup();
    configNvsLoad();

    // BLE starts now; it will be deinited when WiFi connects to free heap for SSL.
    bleProvisionSetup();

    Serial.println(F("[APP] OhmniRobot boot."));
}

// Loop
void loop() {
    // FOC must run continuously; never block.
    actionLoop();
    motionProfileLoop();
    motor.loopFOC();
    motor1.loopFOC();
    motor.move(applyDeadzone(target_left));
    motor1.move(applyDeadzone(target_right));

    // Serial always active in all states
    handleSerial();

    // BLE scan loop ? no-op after bleForceStop()
    bleProvisionLoop();

    switch (s_state) {

        // ?? Check NVS on boot ?????????????????????????????????????????????????
        case INIT: {
            String ssid, pass;
            if (wifiLoadCredentials(ssid, pass)) {
                Serial.printf("[APP] Saved WiFi: %s ? connecting...\n", ssid.c_str());
                wifiBeginConnect(ssid, pass);
                s_wifiStart = millis();
                s_state = WIFI_CONNECTING;
            } else {
                Serial.println(F("[APP] No saved WiFi ? BLE provisioning."));
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
                s_wifiStart = millis();
                s_state = WIFI_CONNECTING;
            }
            break;
        }

        // ?? Wait for WiFi (timeout 15 s -> back to BLE) ???????????????????????
        case WIFI_CONNECTING: {
            if (wifiIsConnected()) {
                onWiFiConnected();
            } else if (millis() - s_wifiStart > WIFI_TIMEOUT_MS) {
                Serial.println(F("[APP] WiFi timeout ? back to BLE provisioning."));
                bleUpdateStatus("WIFI_FAILED");
                s_state = BLE_PROVISIONING;
            }
            break;
        }

        // ?? WiFi OK: HTTP + MCP WebSocket ?????????????????????????????????????
        case RUNNING: {
            if (!s_mcpReady) {
                // Release BLE stack (~90KB) before allocating SSL context (~50KB)
                // Without this, SSL handshake fails due to heap exhaustion
                if (bleIsActive()) {
                    Serial.printf("[APP] Heap before BLE deinit: %u bytes\n", ESP.getFreeHeap());
                    bleForceStop();
                    delay(50);   // let BLE cleanup complete
                    Serial.printf("[APP] Heap after  BLE deinit: %u bytes\n", ESP.getFreeHeap());
                }
                s_robotMcp.begin();
                s_mcpReady = true;
            }

            // Auto-reconnect if WiFi drops
            if (!wifiIsConnected()) {
                Serial.println(F("[APP] WiFi lost ? reconnecting..."));
                wifiReconnect();
                s_wifiStart = millis();
                s_state = WIFI_CONNECTING;
                break;
            }

            httpServerLoop();
            s_robotMcp.loop();
            break;
        }
    }
}

