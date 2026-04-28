#include "config_nvs.h"
#include "../config.h"
#include "../control/motor_control.h"

#include <Preferences.h>

static const char* NVS_NS = "motor-cfg";
static const char* KEY_VALID = "valid";
static const char* KEY_PID_P = "pid_p";
static const char* KEY_PID_I = "pid_i";
static const char* KEY_PID_D = "pid_d";
static const char* KEY_LPF_TF = "lpf_tf";
static const char* KEY_VOLT_LIMIT = "vlim";
static const char* KEY_VEL_LIMIT = "wlim";

bool configNvsHasSaved() {
    Preferences prefs;
    prefs.begin(NVS_NS, true);
    bool ok = prefs.getBool(KEY_VALID, false);
    prefs.end();
    return ok;
}

void configNvsLoad() {
    Preferences prefs;
    prefs.begin(NVS_NS, true);
    bool ok = prefs.getBool(KEY_VALID, false);

    if (!ok) {
        prefs.end();
        motorApplyDefaultConfig();
        Serial.println(F("[CFG] No motor config in NVS. Using compile-time defaults."));
        return;
    }

    float pidP = prefs.getFloat(KEY_PID_P, PID_P);
    float pidI = prefs.getFloat(KEY_PID_I, PID_I);
    float pidD = prefs.getFloat(KEY_PID_D, PID_D);
    float lpfTf = prefs.getFloat(KEY_LPF_TF, LPF_Tf);
    float voltLimit = prefs.getFloat(KEY_VOLT_LIMIT, voltage_limit);
    float velLimit = prefs.getFloat(KEY_VEL_LIMIT, velocity_limit);
    prefs.end();

    motorApplyConfig(pidP, pidI, pidD, lpfTf, voltLimit, velLimit);
    Serial.printf("[CFG] Loaded motor config from NVS: P=%.4f I=%.4f D=%.4f Tf=%.4f Vlim=%.2f Wlim=%.2f\n",
                  pidP, pidI, pidD, lpfTf, voltLimit, velLimit);
}

void configNvsSave() {
    Preferences prefs;
    prefs.begin(NVS_NS, false);
    prefs.putBool(KEY_VALID, true);
    prefs.putFloat(KEY_PID_P, motor.PID_velocity.P);
    prefs.putFloat(KEY_PID_I, motor.PID_velocity.I);
    prefs.putFloat(KEY_PID_D, motor.PID_velocity.D);
    prefs.putFloat(KEY_LPF_TF, motor.LPF_velocity.Tf);
    prefs.putFloat(KEY_VOLT_LIMIT, motor.voltage_limit);
    prefs.putFloat(KEY_VEL_LIMIT, motor.velocity_limit);
    prefs.end();

    Serial.printf("[CFG] Saved motor config to NVS: P=%.4f I=%.4f D=%.4f Tf=%.4f Vlim=%.2f Wlim=%.2f\n",
                  motor.PID_velocity.P, motor.PID_velocity.I, motor.PID_velocity.D,
                  motor.LPF_velocity.Tf, motor.voltage_limit, motor.velocity_limit);
}

void configNvsClear() {
    Preferences prefs;
    prefs.begin(NVS_NS, false);
    prefs.clear();
    prefs.end();

    motorApplyDefaultConfig();
    Serial.println(F("[CFG] Motor config NVS cleared. Compile-time defaults reapplied."));
}
