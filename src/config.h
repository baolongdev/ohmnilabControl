#pragma once

// Compile-time defaults for both BLDC motors.
// Runtime tools/NVS may override several of these values after boot.
const int   pole_pairs           = 7;      // (unit: pair)
const float voltage_power_supply = 17.5;   // (unit: V)
const float voltage_limit        = 10;     // (unit: V)
const float velocity_limit       = 40;     // (unit: rad/s)

// PID velocity
const float PID_P = 0.1;
const float PID_I = 1.0;
const float PID_D = 0.0;

// Low-pass filter
const float LPF_Tf = 0.01;

// I2C bus 0 — left motor sensor
const int I2C0_SDA = 19;
const int I2C0_SCL = 18;

// I2C bus 1 — right motor sensor
const int I2C1_SDA = 23;
const int I2C1_SCL = 5;

// Command shaping for low-speed stability.
// STOP_ZONE snaps very small commands to 0, while DEADZONE lifts non-zero
// commands to a minimum speed so the motors do not buzz without moving.
const float DEADZONE  = 5.0;   // (unit: rad/s) — clamp tối thiểu
const float STOP_ZONE = 1.0;   // (unit: rad/s) — ngưỡng về 0

// Robot geometry for distance / angle motion profiles
const float wheel_radius_m = 0.035f;   // wheel radius in meters
const float wheel_track_m  = 0.180f;   // left-right wheel contact distance in meters

// Final motor command polarity.
// Keep motion semantics (forward/back/left/right) natural in motion.cpp, then
// correct hardware-specific motor inversion here instead of remapping controls.
const float LEFT_MOTOR_SIGN  = -1.0f;
const float RIGHT_MOTOR_SIGN = 1.0f;

// HTTP server
const int HTTP_PORT = 80;

// BLE provisioning — UUIDs
#define BLE_DEVICE_NAME      "OhmniRobot"
#define BLE_SERVICE_UUID     "12345678-0000-1000-8000-00805f9b34fb"
#define BLE_CHAR_SSID_UUID   "12345678-0001-1000-8000-00805f9b34fb"
#define BLE_CHAR_PASS_UUID   "12345678-0002-1000-8000-00805f9b34fb"
#define BLE_CHAR_STATUS_UUID "12345678-0003-1000-8000-00805f9b34fb"

// WiFi — timeout kết nối (ms)
const unsigned long WIFI_TIMEOUT_MS = 15000;

// MCP runtime tuning.
// These values control how long the app waits before warning/retrying MCP when
// BLE has been turned off but heap/network are still not ready for WSS.
const unsigned long MCP_CONNECT_GRACE_MS = 8000;
const unsigned long MCP_MIN_HEAP_WARN = 50000;
const unsigned long MCP_RETRY_INTERVAL_MS = 5000;

// BLE — Command characteristic (0004): write "SCAN" để quét WiFi
#define BLE_CHAR_CMD_UUID    "12345678-0004-1000-8000-00805f9b34fb"

// MCP WebSocket endpoint (XiaoZhi).
// Important: this token grants remote access to the robot MCP endpoint and
// should not be committed to a public repository in production.
#define MCP_ENDPOINT \
    "wss://api.xiaozhi.me/mcp/?token=eyJhbGciOiJFUzI1NiIsInR5cCI6IkpXVCJ9.eyJ1c2VySWQiOjY0NjU4OSwiYWdlbnRJZCI6MTA0NjEyMSwiZW5kcG9pbnRJZCI6ImFnZW50XzEwNDYxMjEiLCJwdXJwb3NlIjoibWNwLWVuZHBvaW50IiwiaWF0IjoxNzc3MzY1MjEyLCJleHAiOjE4MDg5MjI4MTJ9.DCzywKAHZF4Mr2ZQnfsmU0be77AH2FtIaBP3lxngfCtLhPjwyVreTrbDB5ZMUizhgvVKJFpQOyjynaAwYH0dIQ"
