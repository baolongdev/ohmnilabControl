#include "wifi_manager.h"
#include <WiFi.h>
#include <Preferences.h>

static String s_ssid;
static String s_password;

static const char* NVS_NS   = "wifi-cfg";
static const char* NVS_SSID = "ssid";
static const char* NVS_PASS = "pass";

// ── NVS persistence ───────────────────────────────────────────────────────────

void wifiSaveCredentials(const String& ssid, const String& password) {
    Preferences prefs;
    prefs.begin(NVS_NS, false);
    prefs.putString(NVS_SSID, ssid);
    prefs.putString(NVS_PASS, password);
    prefs.end();
    Serial.println("[WiFi] Credentials saved to NVS.");
}

bool wifiLoadCredentials(String& ssid, String& password) {
    Preferences prefs;
    prefs.begin(NVS_NS, true);   // read-only
    ssid     = prefs.getString(NVS_SSID, "");
    password = prefs.getString(NVS_PASS, "");
    prefs.end();
    return ssid.length() > 0;
}

void wifiClearCredentials() {
    Preferences prefs;
    prefs.begin(NVS_NS, false);
    prefs.clear();
    prefs.end();
    Serial.println("[WiFi] NVS credentials cleared.");
}

// ── Connection ────────────────────────────────────────────────────────────────

void wifiBeginConnect(const String& ssid, const String& password) {
    s_ssid     = ssid;
    s_password = password;
    WiFi.mode(WIFI_STA);

    if (password.length() == 0) {
        WiFi.begin(ssid.c_str());
        Serial.print("[WiFi] Connecting to open network: ");
    } else {
        WiFi.begin(ssid.c_str(), password.c_str());
        Serial.print("[WiFi] Connecting to: ");
    }
    Serial.println(ssid);
}

void wifiReconnect() {
    if (s_ssid.length() == 0) return;
    Serial.println("[WiFi] Reconnecting...");
    wifiBeginConnect(s_ssid, s_password);
}

bool wifiIsConnected() {
    return WiFi.status() == WL_CONNECTED;
}

String wifiGetIP()      { return WiFi.localIP().toString(); }
String wifiGetSSID()    { return WiFi.SSID(); }
int    wifiGetRSSI()    { return WiFi.RSSI(); }
String wifiGetGateway() { return WiFi.gatewayIP().toString(); }
