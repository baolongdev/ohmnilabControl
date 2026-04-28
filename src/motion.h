#pragma once

extern float target_left;
extern float target_right;

float applyDeadzone(float v);

void Forward (float spd);
void Backward(float spd);
void TurnLeft (float spd);
void TurnRight(float spd);
void Stop();
