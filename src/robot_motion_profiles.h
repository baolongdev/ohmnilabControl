#pragma once

#include <Arduino.h>

void motionProfileLoop();

bool motionProfileStartTimed(const String& direction, float speed, unsigned long durationMs);
bool motionProfileStartDistance(const String& direction, float speed, float distanceM);
bool motionProfileStartTurnAngle(const String& direction, float speed, float angleDeg);

void motionProfileStop();
bool motionProfileIsRunning();
String motionProfileStatusJson();
