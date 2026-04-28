#include "robot_actions.h"
#include "motion.h"
#include "../config.h"
#include "motor_control.h"

#include <Arduino.h>

enum ActionMode {
    ACTION_NONE,
    ACTION_TURN_LEFT_RIGHT,
    ACTION_FORWARD_BACKWARD,
    ACTION_ZIGZAG,
    ACTION_SCAN_SWEEP,
    ACTION_SPIN_BURST,
    ACTION_PATROL_SHORT,
    ACTION_EMOTION_DISAGREE,
    ACTION_EMOTION_HAPPY,
    ACTION_EMOTION_CURIOUS,
    ACTION_EMOTION_EXCITED,
    ACTION_EMOTION_SHY,
    ACTION_RANDOM_MOVE
};

enum ActionCommand {
    CMD_STOP,
    CMD_FORWARD,
    CMD_BACKWARD,
    CMD_TURN_LEFT,
    CMD_TURN_RIGHT
};

struct ActionStep {
    ActionCommand cmd;
    unsigned long durationMs;
};

static ActionMode s_mode = ACTION_NONE;
static float s_speed = 0.0f;
static unsigned long s_stepStarted = 0;
static unsigned long s_actionStarted = 0;
static unsigned long s_actionDuration = 0;
static int s_stepIndex = 0;
static ActionStep s_steps[12];
static int s_stepCount = 0;
static ActionCommand s_randomCmd = CMD_STOP;
static unsigned long s_randomStepMs = 0;

// Actions are short scripted command sequences layered on top of the same
// motion primitives used by HTTP/serial/MCP. Only one action can run at a time.
static const char* modeName(ActionMode mode) {
    switch (mode) {
        case ACTION_TURN_LEFT_RIGHT: return "ACTION1";
        case ACTION_FORWARD_BACKWARD: return "ACTION2";
        case ACTION_ZIGZAG: return "ACTION3";
        case ACTION_SCAN_SWEEP: return "ACTION4";
        case ACTION_SPIN_BURST: return "ACTION5";
        case ACTION_PATROL_SHORT: return "ACTION6";
        case ACTION_EMOTION_DISAGREE: return "emotion_disagree";
        case ACTION_EMOTION_HAPPY: return "emotion_happy";
        case ACTION_EMOTION_CURIOUS: return "emotion_curious";
        case ACTION_EMOTION_EXCITED: return "emotion_excited";
        case ACTION_EMOTION_SHY: return "emotion_shy";
        case ACTION_RANDOM_MOVE: return "random_move";
        default: return "none";
    }
}

static const char* cmdName(ActionCommand cmd) {
    switch (cmd) {
        case CMD_FORWARD: return "forward";
        case CMD_BACKWARD: return "backward";
        case CMD_TURN_LEFT: return "turn_left";
        case CMD_TURN_RIGHT: return "turn_right";
        default: return "stop";
    }
}

static float clampActionSpeed(float speed) {
    if (speed < 0.0f) speed = -speed;
    if (speed < DEADZONE) speed = DEADZONE;
    float limit = motorRuntimeVelocityLimit();
    if (speed > limit) speed = limit;
    return speed;
}

static unsigned long clampMs(unsigned long value, unsigned long minValue, unsigned long maxValue) {
    if (value < minValue) return minValue;
    if (value > maxValue) return maxValue;
    return value;
}

static void applyCommand(ActionCommand cmd) {
    switch (cmd) {
        case CMD_FORWARD: Forward(s_speed); break;
        case CMD_BACKWARD: Backward(s_speed); break;
        case CMD_TURN_LEFT: TurnLeft(s_speed); break;
        case CMD_TURN_RIGHT: TurnRight(s_speed); break;
        default: Stop(); break;
    }
}

static void beginSequencedAction(ActionMode mode, float speed) {
    // s_steps[] must already be populated by the caller before entering here.
    // This helper only initializes timing/state and kicks off the first step.
    s_mode = mode;
    s_speed = clampActionSpeed(speed);
    s_stepIndex = 0;
    s_stepStarted = millis();
    s_actionStarted = s_stepStarted;
    applyCommand(s_steps[0].cmd);
}

bool actionStartAction1(float speed, unsigned long stepMs) {
    stepMs = clampMs(stepMs, 200, 5000);
    s_steps[0] = {CMD_TURN_LEFT, stepMs};
    s_steps[1] = {CMD_STOP, 250};
    s_steps[2] = {CMD_TURN_RIGHT, stepMs};
    s_steps[3] = {CMD_STOP, 250};
    s_stepCount = 4;
    s_actionDuration = stepMs * 2 + 500;
    beginSequencedAction(ACTION_TURN_LEFT_RIGHT, speed);
    Serial.printf("[ACTION] Start ACTION1 speed=%.1f step=%lu ms\n", s_speed, stepMs);
    return true;
}

bool actionStartAction2(float speed, unsigned long stepMs) {
    stepMs = clampMs(stepMs, 200, 5000);
    s_steps[0] = {CMD_FORWARD, stepMs};
    s_steps[1] = {CMD_STOP, 250};
    s_steps[2] = {CMD_BACKWARD, stepMs};
    s_steps[3] = {CMD_STOP, 250};
    s_stepCount = 4;
    s_actionDuration = stepMs * 2 + 500;
    beginSequencedAction(ACTION_FORWARD_BACKWARD, speed);
    Serial.printf("[ACTION] Start ACTION2 speed=%.1f step=%lu ms\n", s_speed, stepMs);
    return true;
}

bool actionStartAction3(float speed, unsigned long stepMs) {
    stepMs = clampMs(stepMs, 200, 5000);
    s_steps[0] = {CMD_FORWARD, stepMs};
    s_steps[1] = {CMD_TURN_LEFT, stepMs / 2};
    s_steps[2] = {CMD_FORWARD, stepMs};
    s_steps[3] = {CMD_TURN_RIGHT, stepMs / 2};
    s_steps[4] = {CMD_FORWARD, stepMs};
    s_steps[5] = {CMD_STOP, 250};
    s_stepCount = 6;
    s_actionDuration = stepMs * 3 + stepMs + 250;
    beginSequencedAction(ACTION_ZIGZAG, speed);
    Serial.printf("[ACTION] Start ACTION3 speed=%.1f step=%lu ms\n", s_speed, stepMs);
    return true;
}

bool actionStartAction4(float speed, unsigned long stepMs) {
    stepMs = clampMs(stepMs, 200, 5000);
    unsigned long shortStop = 180;
    s_steps[0] = {CMD_TURN_LEFT, stepMs};
    s_steps[1] = {CMD_STOP, shortStop};
    s_steps[2] = {CMD_TURN_RIGHT, stepMs * 2};
    s_steps[3] = {CMD_STOP, shortStop};
    s_steps[4] = {CMD_TURN_LEFT, stepMs};
    s_steps[5] = {CMD_STOP, 250};
    s_stepCount = 6;
    s_actionDuration = stepMs * 4 + shortStop * 2 + 250;
    beginSequencedAction(ACTION_SCAN_SWEEP, speed);
    Serial.printf("[ACTION] Start ACTION4 speed=%.1f step=%lu ms\n", s_speed, stepMs);
    return true;
}

bool actionStartAction5(float speed, unsigned long stepMs) {
    stepMs = clampMs(stepMs, 200, 5000);
    unsigned long burst = stepMs / 2;
    s_steps[0] = {CMD_TURN_LEFT, burst};
    s_steps[1] = {CMD_TURN_RIGHT, burst};
    s_steps[2] = {CMD_TURN_LEFT, burst};
    s_steps[3] = {CMD_TURN_RIGHT, burst};
    s_steps[4] = {CMD_TURN_LEFT, burst};
    s_steps[5] = {CMD_STOP, 250};
    s_stepCount = 6;
    s_actionDuration = burst * 5 + 250;
    beginSequencedAction(ACTION_SPIN_BURST, speed);
    Serial.printf("[ACTION] Start ACTION5 speed=%.1f step=%lu ms\n", s_speed, stepMs);
    return true;
}

bool actionStartAction6(float speed, unsigned long stepMs) {
    stepMs = clampMs(stepMs, 200, 5000);
    unsigned long turnMs = stepMs / 2;
    s_steps[0] = {CMD_FORWARD, stepMs};
    s_steps[1] = {CMD_TURN_LEFT, turnMs};
    s_steps[2] = {CMD_FORWARD, stepMs};
    s_steps[3] = {CMD_TURN_RIGHT, turnMs};
    s_steps[4] = {CMD_BACKWARD, stepMs / 2};
    s_steps[5] = {CMD_STOP, 250};
    s_stepCount = 6;
    s_actionDuration = stepMs * 2 + turnMs * 2 + stepMs / 2 + 250;
    beginSequencedAction(ACTION_PATROL_SHORT, speed);
    Serial.printf("[ACTION] Start ACTION6 speed=%.1f step=%lu ms\n", s_speed, stepMs);
    return true;
}

bool actionStartEmotionDisagree(float speed, unsigned long stepMs) {
    stepMs = clampMs(stepMs, 200, 5000);
    unsigned long shortStop = 140;
    s_steps[0] = {CMD_TURN_LEFT, stepMs};
    s_steps[1] = {CMD_STOP, shortStop};
    s_steps[2] = {CMD_TURN_RIGHT, stepMs};
    s_steps[3] = {CMD_STOP, shortStop};
    s_steps[4] = {CMD_TURN_LEFT, stepMs};
    s_steps[5] = {CMD_STOP, shortStop};
    s_steps[6] = {CMD_TURN_RIGHT, stepMs};
    s_steps[7] = {CMD_STOP, 220};
    s_stepCount = 8;
    s_actionDuration = stepMs * 4 + shortStop * 3 + 220;
    beginSequencedAction(ACTION_EMOTION_DISAGREE, speed);
    Serial.printf("[ACTION] Start emotion_disagree speed=%.1f step=%lu ms\n", s_speed, stepMs);
    return true;
}

bool actionStartEmotionHappy(float speed, unsigned long stepMs) {
    stepMs = clampMs(stepMs, 200, 5000);
    unsigned long spin = stepMs;
    unsigned long shortStop = 120;
    s_steps[0] = {CMD_TURN_LEFT, spin};
    s_steps[1] = {CMD_STOP, shortStop};
    s_steps[2] = {CMD_TURN_LEFT, spin};
    s_steps[3] = {CMD_STOP, shortStop};
    s_steps[4] = {CMD_TURN_LEFT, spin};
    s_steps[5] = {CMD_STOP, 250};
    s_stepCount = 6;
    s_actionDuration = spin * 3 + shortStop * 2 + 250;
    beginSequencedAction(ACTION_EMOTION_HAPPY, speed);
    Serial.printf("[ACTION] Start emotion_happy speed=%.1f step=%lu ms\n", s_speed, stepMs);
    return true;
}

bool actionStartEmotionCurious(float speed, unsigned long stepMs) {
    stepMs = clampMs(stepMs, 200, 5000);
    unsigned long shortStop = 180;
    s_steps[0] = {CMD_TURN_LEFT, stepMs};
    s_steps[1] = {CMD_STOP, shortStop};
    s_steps[2] = {CMD_TURN_RIGHT, stepMs * 2};
    s_steps[3] = {CMD_STOP, shortStop};
    s_steps[4] = {CMD_TURN_LEFT, stepMs};
    s_steps[5] = {CMD_STOP, 250};
    s_stepCount = 6;
    s_actionDuration = stepMs * 4 + shortStop * 2 + 250;
    beginSequencedAction(ACTION_EMOTION_CURIOUS, speed);
    Serial.printf("[ACTION] Start emotion_curious speed=%.1f step=%lu ms\n", s_speed, stepMs);
    return true;
}

bool actionStartEmotionExcited(float speed, unsigned long stepMs) {
    stepMs = clampMs(stepMs, 200, 5000);
    unsigned long turnMs = stepMs / 2;
    s_steps[0] = {CMD_FORWARD, stepMs};
    s_steps[1] = {CMD_TURN_LEFT, turnMs};
    s_steps[2] = {CMD_FORWARD, stepMs};
    s_steps[3] = {CMD_TURN_RIGHT, turnMs};
    s_steps[4] = {CMD_FORWARD, stepMs};
    s_steps[5] = {CMD_TURN_LEFT, turnMs};
    s_steps[6] = {CMD_STOP, 220};
    s_stepCount = 7;
    s_actionDuration = stepMs * 3 + turnMs * 3 + 220;
    beginSequencedAction(ACTION_EMOTION_EXCITED, speed);
    Serial.printf("[ACTION] Start emotion_excited speed=%.1f step=%lu ms\n", s_speed, stepMs);
    return true;
}

bool actionStartEmotionShy(float speed, unsigned long stepMs) {
    stepMs = clampMs(stepMs, 200, 5000);
    unsigned long half = stepMs / 2;
    s_steps[0] = {CMD_BACKWARD, stepMs};
    s_steps[1] = {CMD_STOP, 180};
    s_steps[2] = {CMD_TURN_RIGHT, half};
    s_steps[3] = {CMD_STOP, 180};
    s_steps[4] = {CMD_FORWARD, half};
    s_steps[5] = {CMD_STOP, 250};
    s_stepCount = 6;
    s_actionDuration = stepMs + half + half + 610;
    beginSequencedAction(ACTION_EMOTION_SHY, speed);
    Serial.printf("[ACTION] Start emotion_shy speed=%.1f step=%lu ms\n", s_speed, stepMs);
    return true;
}

static void pickRandomStep() {
    // Random mode deliberately chooses only movement commands here; overall stop
    // behavior is governed by the enclosing action duration and actionStop().
    int pick = random(0, 4);
    s_randomCmd = (ActionCommand)(CMD_FORWARD + pick);
    s_randomStepMs = (unsigned long)random(350, 800);
    s_stepStarted = millis();
    applyCommand(s_randomCmd);
}

bool actionStartRandomMove(float speed, unsigned long minMs, unsigned long maxMs) {
    minMs = clampMs(minMs, 500, 10000);
    maxMs = clampMs(maxMs, minMs, 10000);
    s_mode = ACTION_RANDOM_MOVE;
    s_speed = clampActionSpeed(speed);
    s_actionStarted = millis();
    s_actionDuration = (maxMs == minMs) ? minMs : (unsigned long)random(minMs, maxMs + 1);
    s_stepIndex = 0;
    s_stepCount = 0;
    pickRandomStep();
    Serial.printf("[ACTION] Start random_move speed=%.1f duration=%lu ms\n", s_speed, s_actionDuration);
    return true;
}

void actionStop() {
    if (s_mode != ACTION_NONE) {
        Serial.printf("[ACTION] Stop %s\n", modeName(s_mode));
    }
    s_mode = ACTION_NONE;
    s_stepIndex = 0;
    s_stepCount = 0;
    s_actionDuration = 0;
    s_randomCmd = CMD_STOP;
    Stop();
}

bool actionIsRunning() {
    return s_mode != ACTION_NONE;
}

void actionLoop() {
    if (s_mode == ACTION_NONE) return;

    unsigned long now = millis();

    if (s_mode == ACTION_RANDOM_MOVE) {
        // Random mode has per-step timing plus a separate total action timeout.
        if (now - s_actionStarted >= s_actionDuration) {
            actionStop();
            return;
        }
        if (now - s_stepStarted >= s_randomStepMs) {
            s_stepIndex++;
            pickRandomStep();
        }
        return;
    }

    // Sequenced modes walk a prebuilt table of commands and durations.
    if (s_stepIndex >= s_stepCount) {
        actionStop();
        return;
    }

    if (now - s_stepStarted >= s_steps[s_stepIndex].durationMs) {
        s_stepIndex++;
        if (s_stepIndex >= s_stepCount) {
            actionStop();
            return;
        }
        s_stepStarted = now;
        applyCommand(s_steps[s_stepIndex].cmd);
    }
}

String actionStatusJson() {
    unsigned long now = millis();
    unsigned long elapsed = (s_mode == ACTION_NONE) ? 0 : (now - s_actionStarted);
    unsigned long remaining = 0;
    const char* cmd = "stop";

    if (s_mode == ACTION_RANDOM_MOVE) {
        cmd = cmdName(s_randomCmd);
        remaining = (elapsed >= s_actionDuration) ? 0 : (s_actionDuration - elapsed);
    } else if (s_mode != ACTION_NONE && s_stepIndex < s_stepCount) {
        cmd = cmdName(s_steps[s_stepIndex].cmd);
        remaining = (elapsed >= s_actionDuration) ? 0 : (s_actionDuration - elapsed);
    }

    String r = "{\"ok\":true,\"running\":";
    r += actionIsRunning() ? "true" : "false";
    r += ",\"action\":\"";
    r += modeName(s_mode);
    r += "\",\"current\":\"";
    r += cmd;
    r += "\",\"step\":";
    r += s_stepIndex;
    r += ",\"speed\":";
    r += String(s_speed, 3);
    r += ",\"elapsed_ms\":";
    r += elapsed;
    r += ",\"remaining_ms\":";
    r += remaining;
    r += "}";
    return r;
}
