#pragma once

#include <Arduino.h>

// Runtime-adjustable motor config with NVS persistence.
// Loaded after motorSetup() so NVS values override compile-time defaults.

void configNvsLoad();   // load from NVS, apply to motor & motor1
void configNvsSave();   // save current motor PID/limits to NVS
void configNvsClear();  // erase NVS config, reapply compile-time defaults
bool configNvsHasSaved();
