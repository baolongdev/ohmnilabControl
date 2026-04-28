#include "motion.h"
#include "../config.h"
#include "motor_control.h"
#include <Arduino.h>

float target_left  = 0;
float target_right = 0;

// All high-level command paths normalize speed through this helper so HTTP,
// serial, MCP tools, and scripted actions share the same runtime velocity cap.
static float clampCommandSpeed(float spd) {
  if (spd < 0) spd = -spd;
  float limit = motorRuntimeVelocityLimit();
  if (spd > limit) spd = limit;
  return spd;
}

// Apply the deadzone at the last possible moment before motor.move(). This keeps
// higher-level modules free to work with semantic targets while still protecting
// the physical motors from low-speed buzzing/stall behavior.
float applyDeadzone(float v) {
  if (abs(v) < STOP_ZONE) return 0;
  if (v > 0 && v < DEADZONE)  return  DEADZONE;
  if (v < 0 && v > -DEADZONE) return -DEADZONE;
  return v;
}

// Semantic robot motion mapping. Hardware-specific inversion is applied later
// at the motor output layer via LEFT_MOTOR_SIGN / RIGHT_MOTOR_SIGN.
void Forward(float spd)  { spd = clampCommandSpeed(spd); target_left = -spd; target_right =  spd; }
void Backward(float spd) { spd = clampCommandSpeed(spd); target_left =  spd; target_right = -spd; }
void TurnLeft(float spd) { spd = clampCommandSpeed(spd); target_left =  spd; target_right =  spd; }
void TurnRight(float spd){ spd = clampCommandSpeed(spd); target_left = -spd; target_right = -spd; }
void Stop()              { target_left =    0; target_right =    0; }
