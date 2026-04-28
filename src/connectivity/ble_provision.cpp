#include "ble_provision.h"
#include "wifi_manager.h"
#include "../config.h"

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <WiFi.h>

// ── Internal state ────────────────────────────────────────────────────────────
static bool   s_bleActive       = false;  // true after bleProvisionSetup(), false after bleForceStop()
static String s_ssid;
static String s_password;
static bool   s_hasCredentials  = false;

// Scan state — non-blocking, processed in bleProvisionLoop().
// BLE callbacks only flip flags; all WiFi operations stay on the main loop side
// to avoid crossing BLE task context with WiFi driver calls.
static volatile bool s_scanRequested = false;
static bool          s_scanning      = false;
static int           s_scanCount     = 0;
static int           s_scanIdx       = -1;
static unsigned long s_lastNotify    = 0;

static BLECharacteristic* s_statusChar = nullptr;

// ── Notify helper ─────────────────────────────────────────────────────────────
static void notify(const String& msg) {
    if (!s_bleActive || !s_statusChar) return;
    s_statusChar->setValue(msg.c_str());
    s_statusChar->notify();
}

// ── GATT Callbacks (run in BLE task — only set flags, never call WiFi) ────────
class SSIDCallback : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* pChar) override {
        s_ssid = pChar->getValue().c_str();
        s_hasCredentials = false;
        Serial.print("[BLE] SSID received: ");
        Serial.println(s_ssid);
    }
};

class PasswordCallback : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* pChar) override {
        s_password = pChar->getValue().c_str();

        if (s_password.length() == 0) {
            Serial.println("[BLE] Password: (empty - open network)");
        } else {
            Serial.println("[BLE] Password received.");
        }

        if (s_ssid.length() > 0) {
            wifiSaveCredentials(s_ssid, s_password);
            s_hasCredentials = true;
            notify("CONNECTING");
        } else {
            Serial.println("[BLE] SSID not yet received - skipping connect.");
        }
    }
};

class CommandCallback : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* pChar) override {
        String cmd = pChar->getValue().c_str();
        cmd.trim();
        cmd.toUpperCase();
        Serial.print("[BLE] CMD received: "); Serial.println(cmd);
        if (cmd == "SCAN") {
            s_scanRequested = true;
        }
    }
};

// ── Public API ────────────────────────────────────────────────────────────────
void bleProvisionSetup() {
    if (s_bleActive) return;   // already initialized

    BLEDevice::init(BLE_DEVICE_NAME);
    BLEDevice::setMTU(512);

    BLEServer*  pServer  = BLEDevice::createServer();
    BLEService* pService = pServer->createService(BLEUUID(BLE_SERVICE_UUID), 20);

    BLECharacteristic* pSSID = pService->createCharacteristic(
        BLE_CHAR_SSID_UUID, BLECharacteristic::PROPERTY_WRITE);
    pSSID->setCallbacks(new SSIDCallback());

    BLECharacteristic* pPass = pService->createCharacteristic(
        BLE_CHAR_PASS_UUID, BLECharacteristic::PROPERTY_WRITE);
    pPass->setCallbacks(new PasswordCallback());

    s_statusChar = pService->createCharacteristic(
        BLE_CHAR_STATUS_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
    s_statusChar->addDescriptor(new BLE2902());
    s_statusChar->setValue("WAITING");

    BLECharacteristic* pCmd = pService->createCharacteristic(
        BLE_CHAR_CMD_UUID, BLECharacteristic::PROPERTY_WRITE);
    pCmd->setCallbacks(new CommandCallback());

    pService->start();

    BLEAdvertising* pAdv = BLEDevice::getAdvertising();
    pAdv->addServiceUUID(BLE_SERVICE_UUID);
    pAdv->setScanResponse(true);
    BLEDevice::startAdvertising();

    s_bleActive = true;

    Serial.println("[BLE] Advertising as \"" BLE_DEVICE_NAME "\"");
    Serial.println("[BLE] UUID: " BLE_SERVICE_UUID);
    Serial.println("[BLE]   0001=SSID  0002=PASS  0003=STATUS  0004=CMD");
}

// BLE and WiFi scans share the same radio, so scan results are streamed back to
// the BLE client gradually from loop() instead of doing any blocking work in a
// callback.
void bleProvisionLoop() {
    if (!s_bleActive) return;

    if (s_scanRequested) {
        s_scanRequested = false;
        bleStartScan();
    }

    if (s_scanning) {
        int n = WiFi.scanComplete();
        if (n == WIFI_SCAN_RUNNING) return;

        s_scanning = false;

        if (n == WIFI_SCAN_FAILED || n < 0) {
            notify("SCAN_ERR");
            Serial.println("[WiFiScan] Scan failed.");
            return;
        }

        s_scanCount = n;
        s_scanIdx   = 0;
        notify("SCAN_START:" + String(n));
        Serial.printf("[WiFiScan] Found %d networks.\n", n);
        return;
    }

    if (s_scanIdx >= 0 && s_scanIdx < s_scanCount) {
        if (millis() - s_lastNotify < 60) return;
        s_lastNotify = millis();

        String enc;
        wifi_auth_mode_t auth = WiFi.encryptionType(s_scanIdx);
        if      (auth == WIFI_AUTH_OPEN)    enc = "OPEN";
        else if (auth == WIFI_AUTH_WPA_PSK) enc = "WPA";
        else                                enc = "WPA2";

        String net = "SCAN:" + WiFi.SSID(s_scanIdx)
                   + "|" + String(WiFi.RSSI(s_scanIdx))
                   + "|" + enc;
        notify(net);
        Serial.printf("[WiFiScan] %d/%d: %s\n", s_scanIdx + 1, s_scanCount, net.c_str());
        s_scanIdx++;
        return;
    }

    if (s_scanIdx == s_scanCount && s_scanCount > 0) {
        notify("SCAN_END:" + String(s_scanCount));
        WiFi.scanDelete();
        Serial.println("[WiFiScan] Done. Results sent via BLE.");
        s_scanning = false;
        s_scanRequested = false;
        s_lastNotify = 0;
        s_scanIdx   = -1;
        s_scanCount = 0;
    }
}

void bleStartScan() {
    if (!s_bleActive) return;
    if (s_scanning || s_scanIdx >= 0) {
        Serial.println("[WiFiScan] Already scanning, skipped.");
        return;
    }
    // Do not tear down an active STA session just to perform a BLE-triggered
    // scan; runtime operation has higher priority than provisioning discovery.
    if (WiFi.status() == WL_CONNECTED) {
        notify("SCAN_BUSY");
        Serial.println("[WiFiScan] Skipped because WiFi is connected.");
        return;
    }
    Serial.println("[WiFiScan] Starting async scan...");
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(false);
    WiFi.scanNetworks(/*async=*/true);
    s_scanning = true;
    notify("SCANNING");
}

void bleUpdateStatus(const String& status) {
    if (!s_bleActive) return;
    notify(status);
    Serial.print("[BLE] Status -> ");
    Serial.println(status);
}

void bleProvisionStop() {
    if (!s_bleActive) return;
    BLEDevice::stopAdvertising();
    Serial.println("[BLE] Advertising stopped.");
}

void bleStartAdvertising() {
    if (!s_bleActive) return;
    BLEDevice::startAdvertising();
    Serial.println("[BLE] Advertising restarted.");
}

void bleForceStop() {
    if (!s_bleActive) return;
    // Clear scan bookkeeping before deinit so a future bleProvisionSetup()
    // always starts from a clean provisioning state.
    s_scanRequested = false;
    s_scanning = false;
    s_scanCount = 0;
    s_scanIdx = -1;
    s_lastNotify = 0;
    WiFi.scanDelete();
    BLEDevice::deinit(true);
    s_bleActive  = false;
    s_statusChar = nullptr;
    Serial.printf("[BLE] BLE deinit. Free heap: %u bytes.\n", ESP.getFreeHeap());
}

bool bleIsActive() { return s_bleActive; }

bool bleHasCredentials() { return s_hasCredentials; }
String bleGetSSID()       { return s_ssid; }
String bleGetPassword()   { return s_password; }

bool bleConsumeCredentials(String& ssid, String& password) {
    if (!s_hasCredentials) return false;
    ssid     = s_ssid;
    password = s_password;
    s_hasCredentials = false;
    return true;
}
