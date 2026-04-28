#include "motion.h"
#include "config.h"
#include "motor_control.h"
#include <Arduino.h>

float target_left  = 0;
float target_right = 0;

static float clampCommandSpeed(float spd) {
  if (spd < 0) spd = -spd;
  float limit = motorRuntimeVelocityLimit();
  if (spd > limit) spd = limit;
  return spd;
}

// Clamp gia tri vao deadzone de tranh motor rung khi toc do thap
float applyDeadzone(float v) {
  if (abs(v) < STOP_ZONE) return 0;
  if (v > 0 && v < DEADZONE)  return  DEADZONE;
  if (v < 0 && v > -DEADZONE) return -DEADZONE;
  return v;
}

void Forward(float spd)  { spd = clampCommandSpeed(spd); target_left = -spd; target_right =  spd; }
void Backward(float spd) { spd = clampCommandSpeed(spd); target_left =  spd; target_right = -spd; }
void TurnLeft(float spd) { spd = clampCommandSpeed(spd); target_left =  spd; target_right =  spd; }
void TurnRight(float spd){ spd = clampCommandSpeed(spd); target_left = -spd; target_right = -spd; }
void Stop()              { target_left =    0; target_right =    0; }
