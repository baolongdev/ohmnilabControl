#pragma once
#include <Arduino.h>

void   wifiBeginConnect(const String& ssid, const String& password);
void   wifiReconnect();
bool   wifiIsConnected();
String wifiGetIP();
String wifiGetSSID();
int    wifiGetRSSI();
String wifiGetGateway();

// NVS credential persistence (Preferences)
void wifiSaveCredentials(const String& ssid, const String& password);
bool wifiLoadCredentials(String& ssid, String& password);
void wifiClearCredentials();
