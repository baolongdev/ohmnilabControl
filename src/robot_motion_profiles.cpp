#include "robot_motion_profiles.h"

#include "config.h"
#include "motion.h"
#include "motor_control.h"

#include <Arduino.h>

enum MotionProfileMode {
    PROFILE_NONE,
    PROFILE_TIMED,
    PROFILE_DISTANCE,
    PROFILE_TURN_ANGLE
};

enum MotionProfileCommand {
    PROFILE_CMD_STOP,
    PROFILE_CMD_FORWARD,
    PROFILE_CMD_BACKWARD,
    PROFILE_CMD_TURN_LEFT,
    PROFILE_CMD_TURN_RIGHT
};

static MotionProfileMode s_mode = PROFILE_NONE;
static MotionProfileCommand s_cmd = PROFILE_CMD_STOP;
static float s_speed = 0.0f;
static unsigned long s_startedMs = 0;
static unsigned long s_durationMs = 0;
static float s_targetDistanceM = 0.0f;
static float s_targetAngleDeg = 0.0f;
static float s_leftStartAngle = 0.0f;
static float s_rightStartAngle = 0.0f;

static const char* modeName(MotionProfileMode mode) {
    switch (mode) {
        case PROFILE_TIMED: return "timed_move";
        case PROFILE_DISTANCE: return "distance_move";
        case PROFILE_TURN_ANGLE: return "turn_angle";
        default: return "none";
    }
}

static const char* cmdName(MotionProfileCommand cmd) {
    switch (cmd) {
        case PROFILE_CMD_FORWARD: return "forward";
        case PROFILE_CMD_BACKWARD: return "backward";
        case PROFILE_CMD_TURN_LEFT: return "turn_left";
        case PROFILE_CMD_TURN_RIGHT: return "turn_right";
        default: return "stop";
    }
}

static bool parseCommand(const String& direction, MotionProfileCommand* out) {
    if (!out) return false;

    String value = direction;
    value.toLowerCase();

    if (value == "forward") {
        *out = PROFILE_CMD_FORWARD;
        return true;
    }
    if (value == "backward") {
        *out = PROFILE_CMD_BACKWARD;
        return true;
    }
    if (value == "left" || value == "turn_left") {
        *out = PROFILE_CMD_TURN_LEFT;
        return true;
    }
    if (value == "right" || value == "turn_right") {
        *out = PROFILE_CMD_TURN_RIGHT;
        return true;
    }
    return false;
}

static bool isLinearCommand(MotionProfileCommand cmd) {
    return cmd == PROFILE_CMD_FORWARD || cmd == PROFILE_CMD_BACKWARD;
}

static bool isTurnCommand(MotionProfileCommand cmd) {
    return cmd == PROFILE_CMD_TURN_LEFT || cmd == PROFILE_CMD_TURN_RIGHT;
}

static float clampProfileSpeed(float speed) {
    if (speed < 0.0f) speed = -speed;
    if (speed < DEADZONE) speed = DEADZONE;
    float limit = motorRuntimeVelocityLimit();
    if (speed > limit) speed = limit;
    return speed;
}

static void applyCommand(MotionProfileCommand cmd) {
    switch (cmd) {
        case PROFILE_CMD_FORWARD: Forward(s_speed); break;
        case PROFILE_CMD_BACKWARD: Backward(s_speed); break;
        case PROFILE_CMD_TURN_LEFT: TurnLeft(s_speed); break;
        case PROFILE_CMD_TURN_RIGHT: TurnRight(s_speed); break;
        default: Stop(); break;
    }
}

static void captureStartPose() {
    s_leftStartAngle = motor.shaft_angle;
    s_rightStartAngle = motor1.shaft_angle;
}

static float leftTravelMeters() {
    return -(motor.shaft_angle - s_leftStartAngle) * wheel_radius_m;
}

static float rightTravelMeters() {
    return (motor1.shaft_angle - s_rightStartAngle) * wheel_radius_m;
}

static float forwardProgressMeters() {
    return (leftTravelMeters() + rightTravelMeters()) * 0.5f;
}

static float headingProgressDeg() {
    float headingRad = (rightTravelMeters() - leftTravelMeters()) / wheel_track_m;
    return headingRad * 180.0f / PI;
}

bool motionProfileStartTimed(const String& direction, float speed, unsigned long durationMs) {
    MotionProfileCommand cmd = PROFILE_CMD_STOP;
    if (!parseCommand(direction, &cmd)) return false;

    durationMs = durationMs < 100 ? 100 : durationMs;
    if (durationMs > 60000) durationMs = 60000;

    s_mode = PROFILE_TIMED;
    s_cmd = cmd;
    s_speed = clampProfileSpeed(speed);
    s_durationMs = durationMs;
    s_targetDistanceM = 0.0f;
    s_targetAngleDeg = 0.0f;
    s_startedMs = millis();
    captureStartPose();
    applyCommand(s_cmd);
    Serial.printf("[PROFILE] Start timed_move cmd=%s speed=%.1f duration=%lu ms\n",
                  cmdName(s_cmd), s_speed, s_durationMs);
    return true;
}

bool motionProfileStartDistance(const String& direction, float speed, float distanceM) {
    MotionProfileCommand cmd = PROFILE_CMD_STOP;
    if (!parseCommand(direction, &cmd) || !isLinearCommand(cmd)) return false;
    if (!isfinite(distanceM) || distanceM <= 0.0f) return false;

    s_mode = PROFILE_DISTANCE;
    s_cmd = cmd;
    s_speed = clampProfileSpeed(speed);
    s_durationMs = 0;
    s_targetDistanceM = distanceM;
    s_targetAngleDeg = 0.0f;
    s_startedMs = millis();
    captureStartPose();
    applyCommand(s_cmd);
    Serial.printf("[PROFILE] Start distance_move cmd=%s speed=%.1f distance=%.3f m\n",
                  cmdName(s_cmd), s_speed, s_targetDistanceM);
    return true;
}

bool motionProfileStartTurnAngle(const String& direction, float speed, float angleDeg) {
    MotionProfileCommand cmd = PROFILE_CMD_STOP;
    if (!parseCommand(direction, &cmd) || !isTurnCommand(cmd)) return false;
    if (!isfinite(angleDeg) || angleDeg <= 0.0f) return false;

    s_mode = PROFILE_TURN_ANGLE;
    s_cmd = cmd;
    s_speed = clampProfileSpeed(speed);
    s_durationMs = 0;
    s_targetDistanceM = 0.0f;
    s_targetAngleDeg = angleDeg;
    s_startedMs = millis();
    captureStartPose();
    applyCommand(s_cmd);
    Serial.printf("[PROFILE] Start turn_angle cmd=%s speed=%.1f angle=%.1f deg\n",
                  cmdName(s_cmd), s_speed, s_targetAngleDeg);
    return true;
}

void motionProfileStop() {
    if (s_mode != PROFILE_NONE) {
        Serial.printf("[PROFILE] Stop %s\n", modeName(s_mode));
    }
    s_mode = PROFILE_NONE;
    s_cmd = PROFILE_CMD_STOP;
    s_durationMs = 0;
    s_targetDistanceM = 0.0f;
    s_targetAngleDeg = 0.0f;
    Stop();
}

bool motionProfileIsRunning() {
    return s_mode != PROFILE_NONE;
}

void motionProfileLoop() {
    if (s_mode == PROFILE_NONE) return;

    unsigned long elapsedMs = millis() - s_startedMs;
    if (s_mode == PROFILE_TIMED) {
        if (elapsedMs >= s_durationMs) {
            motionProfileStop();
        }
        return;
    }

    if (s_mode == PROFILE_DISTANCE) {
        if (fabsf(forwardProgressMeters()) >= s_targetDistanceM) {
            motionProfileStop();
        }
        return;
    }

    if (s_mode == PROFILE_TURN_ANGLE) {
        if (fabsf(headingProgressDeg()) >= s_targetAngleDeg) {
            motionProfileStop();
        }
    }
}

String motionProfileStatusJson() {
    unsigned long elapsedMs = (s_mode == PROFILE_NONE) ? 0 : (millis() - s_startedMs);
    float progressDistanceM = 0.0f;
    float progressAngleDeg = 0.0f;
    unsigned long remainingMs = 0;

    if (s_mode == PROFILE_TIMED) {
        remainingMs = (elapsedMs >= s_durationMs) ? 0 : (s_durationMs - elapsedMs);
    }
    if (s_mode == PROFILE_DISTANCE) {
        progressDistanceM = fabsf(forwardProgressMeters());
    }
    if (s_mode == PROFILE_TURN_ANGLE) {
        progressAngleDeg = fabsf(headingProgressDeg());
    }

    String r = "{\"ok\":true,\"running\":";
    r += motionProfileIsRunning() ? "true" : "false";
    r += ",\"profile\":\"";
    r += modeName(s_mode);
    r += "\",\"command\":\"";
    r += cmdName(s_cmd);
    r += "\",\"speed\":";
    r += String(s_speed, 3);
    r += ",\"elapsed_ms\":";
    r += elapsedMs;
    r += ",\"remaining_ms\":";
    r += remainingMs;
    r += ",\"target_distance_m\":";
    r += String(s_targetDistanceM, 4);
    r += ",\"progress_distance_m\":";
    r += String(progressDistanceM, 4);
    r += ",\"target_angle_deg\":";
    r += String(s_targetAngleDeg, 3);
    r += ",\"progress_angle_deg\":";
    r += String(progressAngleDeg, 3);
    r += "}";
    return r;
}
