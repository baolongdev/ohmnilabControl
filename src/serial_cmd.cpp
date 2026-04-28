#include "serial_cmd.h"
#include "motion.h"
#include "motor_control.h"
#include "wifi_manager.h"
#include "config.h"
#include "config_nvs.h"
#include "robot_actions.h"
#include "robot_motion_profiles.h"

#include <Arduino.h>
#include <WiFi.h>

extern const char* getAppStateStr();

// ── Format helpers ────────────────────────────────────────────────────────────
static void hr() {
    Serial.println(F("============================================================"));
}

static void section(const char* title) {
    Serial.println();
    Serial.print(F("  ["));
    Serial.print(title);
    Serial.println(F("]"));
}

// ── STATUS ────────────────────────────────────────────────────────────────────
static void printStatus() {
    hr();
    Serial.println(F("          OhmniRobot - System Status"));
    hr();

    // ── System ───────────────────────────────────────────────────────────────
    section("SYSTEM");
    Serial.printf("    Uptime        : %lu ms  (%lu s)\n",
                  millis(), millis() / 1000);
    Serial.printf("    Free Heap     : %u bytes\n",   ESP.getFreeHeap());
    Serial.printf("    Min Free Heap : %u bytes\n",   ESP.getMinFreeHeap());
    Serial.printf("    CPU Freq      : %u MHz\n",     ESP.getCpuFreqMHz());
    Serial.printf("    App State     : %s\n",         getAppStateStr());

    // ── Network ───────────────────────────────────────────────────────────────
    section("NETWORK");
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("    Status        : CONNECTED\n");
        Serial.printf("    SSID          : %s\n",     WiFi.SSID().c_str());
        Serial.printf("    IP            : %s\n",     WiFi.localIP().toString().c_str());
        Serial.printf("    Gateway       : %s\n",     WiFi.gatewayIP().toString().c_str());
        Serial.printf("    RSSI          : %d dBm\n", WiFi.RSSI());
        Serial.printf("    HTTP URL      : http://%s/\n",
                      WiFi.localIP().toString().c_str());
    } else {
        Serial.printf("    Status        : DISCONNECTED\n");
    }

    // ── Saved credentials ─────────────────────────────────────────────────────
    section("SAVED WIFI (NVS)");
    String savedSsid, savedPass;
    if (wifiLoadCredentials(savedSsid, savedPass)) {
        Serial.printf("    Saved SSID    : %s\n", savedSsid.c_str());
        Serial.printf("    Saved Pass    : %s\n",
                      savedPass.length() > 0 ? "****" : "(open)");
    } else {
        Serial.println("    No credentials saved.");
    }

    // ── Motion targets ────────────────────────────────────────────────────────
    section("MOTION TARGETS");
    Serial.printf("    target_left   : %+.2f rad/s\n", target_left);
    Serial.printf("    target_right  : %+.2f rad/s\n", target_right);

    const char* motion = "STOP";
    if      (target_left < -0.5f && target_right >  0.5f) motion = "FORWARD";
    else if (target_left >  0.5f && target_right < -0.5f) motion = "BACKWARD";
    else if (target_left >  0.5f && target_right >  0.5f) motion = "TURN LEFT";
    else if (target_left < -0.5f && target_right < -0.5f) motion = "TURN RIGHT";
    else if (fabsf(target_left) > 0.5f || fabsf(target_right) > 0.5f) motion = "MIXED";
    Serial.printf("    Direction     : %s\n", motion);

    // ── Motor LEFT ────────────────────────────────────────────────────────────
    section("MOTOR LEFT  (motor)");
    Serial.printf("    Target Vel    : %+.3f rad/s\n", motor.target);
    Serial.printf("    Actual Vel    : %+.3f rad/s\n", motor.shaft_velocity);
    Serial.printf("    Vel Error     : %+.3f rad/s\n",
                  motor.target - motor.shaft_velocity);
    Serial.printf("    Shaft Angle   : %.4f rad  (%.2f deg)\n",
                  motor.shaft_angle, motor.shaft_angle * 180.0f / PI);
    Serial.printf("    Voltage Q     : %+.3f V\n",     motor.voltage.q);
    Serial.printf("    Voltage D     : %+.3f V\n",     motor.voltage.d);

    // ── Motor RIGHT ───────────────────────────────────────────────────────────
    section("MOTOR RIGHT (motor1)");
    Serial.printf("    Target Vel    : %+.3f rad/s\n", motor1.target);
    Serial.printf("    Actual Vel    : %+.3f rad/s\n", motor1.shaft_velocity);
    Serial.printf("    Vel Error     : %+.3f rad/s\n",
                  motor1.target - motor1.shaft_velocity);
    Serial.printf("    Shaft Angle   : %.4f rad  (%.2f deg)\n",
                  motor1.shaft_angle, motor1.shaft_angle * 180.0f / PI);
    Serial.printf("    Voltage Q     : %+.3f V\n",     motor1.voltage.q);
    Serial.printf("    Voltage D     : %+.3f V\n",     motor1.voltage.d);

    // ── PID ───────────────────────────────────────────────────────────────────
    section("PID VELOCITY");
    Serial.printf("    P             : %.4f\n", motor.PID_velocity.P);
    Serial.printf("    I             : %.4f\n", motor.PID_velocity.I);
    Serial.printf("    D             : %.4f\n", motor.PID_velocity.D);
    Serial.printf("    LPF Tf        : %.4f s\n", motor.LPF_velocity.Tf);

    // ── Config ────────────────────────────────────────────────────────────────
    section("CONFIG");
    Serial.printf("    Pole Pairs    : %d\n",     pole_pairs);
    Serial.printf("    Supply Volt   : %.1f V\n", voltage_power_supply);
    Serial.printf("    Voltage Limit : %.1f V\n", motor.voltage_limit);
    Serial.printf("    Vel Limit     : %.1f rad/s\n", motor.velocity_limit);
    Serial.printf("    NVS Persisted : %s\n", configNvsHasSaved() ? "YES" : "NO");
    Serial.printf("    Deadzone      : %.1f rad/s\n", DEADZONE);
    Serial.printf("    Stop Zone     : %.1f rad/s\n", STOP_ZONE);

    Serial.println();
    hr();
}

// ── HELP ──────────────────────────────────────────────────────────────────────
static void printHelp() {
    Serial.println(F(""));
    Serial.println(F("  OhmniRobot Serial Commands"));
    Serial.println(F("  ------------------------------------------"));
    Serial.println(F("  F<speed>   Forward        e.g. F20"));
    Serial.println(F("  B<speed>   Backward       e.g. B15"));
    Serial.println(F("  L<speed>   Turn Left      e.g. L10"));
    Serial.println(F("  R<speed>   Turn Right     e.g. R10"));
    Serial.println(F("  S          Stop"));
    Serial.println(F("  A1         Action: turn left then right"));
    Serial.println(F("  A2         Action: forward then backward"));
    Serial.println(F("  A3         Action: zigzag short"));
    Serial.println(F("  A4         Action: scan sweep"));
    Serial.println(F("  A5         Action: spin burst"));
    Serial.println(F("  A6         Action: short patrol"));
    Serial.println(F("  AR         Action: random move 2-3s"));
    Serial.println(F("  AS         Stop current action"));
    Serial.println(F("  ?          Print full status"));
    Serial.println(F("  H          Print this help"));
    Serial.println(F("  CLEAR      Clear WiFi NVS + reboot into BLE provisioning"));
    Serial.println(F("  ------------------------------------------"));
    Serial.printf( "  Speed range: %.0f - %.0f rad/s\n", DEADZONE, motorRuntimeVelocityLimit());
    Serial.println(F(""));
}

// ── Public ────────────────────────────────────────────────────────────────────
void handleSerial() {
    if (!Serial.available()) return;

    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd.length() == 0) return;

    Serial.print(F(">> "));
    Serial.println(cmd);

    // Full-word commands (case-insensitive)
    String up = cmd;
    up.toUpperCase();

    if (up == "?" || up == "STATUS") { printStatus(); return; }
    if (up == "H" || up == "HELP")   { printHelp();   return; }
    if (up == "A1") { motionProfileStop(); actionStartAction1(); Serial.println(actionStatusJson()); return; }
    if (up == "A2") { motionProfileStop(); actionStartAction2(); Serial.println(actionStatusJson()); return; }
    if (up == "A3") { motionProfileStop(); actionStartAction3(); Serial.println(actionStatusJson()); return; }
    if (up == "A4") { motionProfileStop(); actionStartAction4(); Serial.println(actionStatusJson()); return; }
    if (up == "A5") { motionProfileStop(); actionStartAction5(); Serial.println(actionStatusJson()); return; }
    if (up == "A6") { motionProfileStop(); actionStartAction6(); Serial.println(actionStatusJson()); return; }
    if (up == "AR") { motionProfileStop(); actionStartRandomMove(); Serial.println(actionStatusJson()); return; }
    if (up == "AS") { actionStop(); Serial.println(actionStatusJson()); return; }

    if (up == "CLEAR") {
        wifiClearCredentials();
        Serial.println(F("[CMD] WiFi credentials cleared. Rebooting into BLE provisioning..."));
        delay(200);
        ESP.restart();
        return;
    }

    // Movement commands — accept upper and lower case
    char  dir = toupper((unsigned char)cmd.charAt(0));
    float spd = cmd.substring(1).toFloat();

    switch (dir) {
        case 'F':
            actionStop();
            motionProfileStop();
            Forward(spd);
            Serial.printf("[CMD] Forward   %+.1f rad/s"
                          "  (L=%.1f R=%.1f)\n", spd, target_left, target_right);
            break;
        case 'B':
            actionStop();
            motionProfileStop();
            Backward(spd);
            Serial.printf("[CMD] Backward  %+.1f rad/s"
                          "  (L=%.1f R=%.1f)\n", spd, target_left, target_right);
            break;
        case 'L':
            actionStop();
            motionProfileStop();
            TurnLeft(spd);
            Serial.printf("[CMD] Turn Left %+.1f rad/s"
                          "  (L=%.1f R=%.1f)\n", spd, target_left, target_right);
            break;
        case 'R':
            actionStop();
            motionProfileStop();
            TurnRight(spd);
            Serial.printf("[CMD] Turn Right%+.1f rad/s"
                          "  (L=%.1f R=%.1f)\n", spd, target_left, target_right);
            break;
        case 'S':
            actionStop();
            motionProfileStop();
            Serial.println(F("[CMD] Stop  (L=0.0 R=0.0)"));
            break;
        default:
            Serial.printf("[ERR] Unknown command: \"%s\"  - send H for help\n",
                          cmd.c_str());
    }
}
