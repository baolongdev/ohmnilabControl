#pragma once

#include <Arduino.h>

void actionLoop();
bool actionStartAction1(float speed = 15.0f, unsigned long stepMs = 900);
bool actionStartAction2(float speed = 18.0f, unsigned long stepMs = 1000);
bool actionStartAction3(float speed = 15.0f, unsigned long stepMs = 700);
bool actionStartAction4(float speed = 16.0f, unsigned long stepMs = 600);
bool actionStartAction5(float speed = 14.0f, unsigned long stepMs = 500);
bool actionStartAction6(float speed = 16.0f, unsigned long stepMs = 650);
bool actionStartEmotionDisagree(float speed = 15.0f, unsigned long stepMs = 450);
bool actionStartEmotionHappy(float speed = 16.0f, unsigned long stepMs = 700);
bool actionStartEmotionCurious(float speed = 15.0f, unsigned long stepMs = 550);
bool actionStartEmotionExcited(float speed = 17.0f, unsigned long stepMs = 450);
bool actionStartEmotionShy(float speed = 12.0f, unsigned long stepMs = 500);
bool actionStartRandomMove(float speed = 15.0f, unsigned long minMs = 2000, unsigned long maxMs = 3000);
void actionStop();
bool actionIsRunning();
String actionStatusJson();
