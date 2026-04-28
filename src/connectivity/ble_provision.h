#pragma once
#include <Arduino.h>

void bleProvisionSetup();

// Called in loop() — handles scan results non-blocking
void bleProvisionLoop();

// Trigger async WiFi scan
void bleStartScan();

// Send status notification to client
void bleUpdateStatus(const String& status);

// Stop advertising (soft stop — BLE stack still in memory)
void bleProvisionStop();

// Restart advertising
void bleStartAdvertising();

// Hard stop — deinit BLE stack, frees ~90KB heap for SSL
// After this call, all BLE functions become no-ops
void bleForceStop();

bool bleIsActive();

// Credential accessors
bool   bleHasCredentials();
String bleGetSSID();
String bleGetPassword();

// Atomically read + clear credentials
bool bleConsumeCredentials(String& ssid, String& password);
