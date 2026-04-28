# OhmniRobot Architecture

## Overview

This project runs a 2-wheel differential-drive robot on ESP32 using SimpleFOC.
The firmware is organized around a small set of runtime subsystems:

- Motor/FOC control for the two BLDC wheels
- Motion primitives and scripted robot behaviors
- BLE provisioning for WiFi onboarding and recovery
- WiFi runtime with HTTP control and MCP over WebSocket
- NVS-backed persistence for WiFi credentials and motor tuning

The most important design constraint is memory pressure on ESP32 when BLE, WiFi,
HTTP, and secure WebSocket are all active at the same time. Because of that,
BLE is used for provisioning and recovery, then turned off during stable WiFi
runtime so MCP has enough heap for WSS.

For setup, flashing, and usage instructions, see [`README.md`](./README.md).

## Folder Layout

### `src/app`

- `robot_drive.cpp`

Owns the top-level application state machine and main `setup()` / `loop()`.
This is the orchestration layer that wires all other modules together.

### `src/control`

- `motor_control.h/.cpp`
- `motion.h/.cpp`
- `robot_actions.h/.cpp`
- `robot_motion_profiles.h/.cpp`

Contains the robot motion stack:

- Low-level motor and sensor initialization
- High-level velocity targets for left/right wheels
- Scripted actions such as `ACTION1`, `emotion_happy`, `random_move`
- Non-blocking motion profiles such as timed move, distance move, and turn angle

### `src/connectivity`

- `ble_provision.h/.cpp`
- `wifi_manager.h/.cpp`

Contains connectivity lifecycle logic:

- BLE GATT service for WiFi provisioning
- BLE-triggered WiFi scan flow
- WiFi connect/reconnect helpers
- NVS-backed WiFi credential storage

### `src/interface`

- `http_server.h/.cpp`
- `serial_cmd.h/.cpp`

Contains local/external user interfaces:

- HTTP control endpoints for the web UI
- Serial terminal commands for bring-up, diagnostics, and recovery

### `src/integrations`

- `robot_mcp.h/.cpp`

Contains third-party runtime integration logic.
Currently this is the XiaoZhi MCP client over WebSocket.

### `src/storage`

- `config_nvs.h/.cpp`

Contains persistence for runtime-adjustable motor parameters.

### `src/config.h`

Central compile-time configuration used across modules.
This includes:

- Motor defaults
- Robot geometry
- BLE UUIDs
- WiFi timeout
- MCP retry tuning
- MCP endpoint

## Runtime Flow

## 1. Boot

`setup()` performs the following high-level sequence:

1. Start serial
2. Initialize motor drivers and sensors
3. Load persisted motor config from NVS
4. Start BLE provisioning service

At this point the robot can already accept BLE provisioning requests.

## 2. App State Machine

The main state machine lives in `src/app/robot_drive.cpp`.

### `INIT`

- Load saved WiFi credentials from NVS
- If credentials exist, begin WiFi connect
- Otherwise enter BLE provisioning mode

### `BLE_PROVISIONING`

- Wait for SSID/password sent over BLE
- BLE may also trigger WiFi scanning for UI-assisted network selection
- Once credentials are received, start WiFi connect

### `WIFI_CONNECTING`

- Poll WiFi connection status without blocking the control loop
- On success, transition to `RUNNING`
- On timeout, re-enable BLE provisioning flow

### `RUNNING`

- Disable BLE to free heap
- Start HTTP server
- Start MCP client
- Keep FOC, MCP, and HTTP processing alive in loop
- If WiFi is lost, re-enable BLE and transition back to reconnect flow

## Fast Control Loop

These calls run on every `loop()` iteration and are treated as the real-time core:

1. `actionLoop()`
2. `motionProfileLoop()`
3. `motor.loopFOC()` for each motor
4. `motor.move(applyDeadzone(target_*))`

Everything else in the system ultimately feeds target wheel velocities into this
loop. No higher-level module should block it with long delays.

## Motion Layers

The motion system is intentionally layered.

### Layer 1: Direct wheel targets

`src/control/motion.cpp`

Defines the semantic robot commands:

- `Forward()`
- `Backward()`
- `TurnLeft()`
- `TurnRight()`
- `Stop()`

These functions translate robot-level movement into signed left/right wheel
velocity targets.

### Layer 2: Scripted actions

`src/control/robot_actions.cpp`

Implements reusable short choreographies such as:

- turn left/right
- forward/backward pulse
- scan sweep
- emotion-like gestures
- random move

Actions are non-blocking and executed as timed step sequences.

### Layer 3: Motion profiles

`src/control/robot_motion_profiles.cpp`

Implements non-blocking goal-based motions such as:

- move for a fixed duration
- move for a target distance
- turn for a target angle

Distance and angle are estimated from wheel travel using robot geometry.

## Control Interfaces

Multiple control surfaces feed the same motion stack.

### BLE provisioning UI

- Used only for onboarding/recovery
- Sends SSID/password
- Receives provisioning status and scan results

### HTTP

- Runtime control interface used by `robot_control.html`
- Supports direct movement endpoints and `/status`
- Manual HTTP commands stop scripted actions/profiles first

### Serial

- Bring-up and diagnostics tool
- Supports direct motion commands, action triggers, status print, and recovery

### MCP

- High-level remote control path over WSS
- Registers movement, config, action, and motion-profile tools

All of these eventually converge on the same motion primitives and motor loop.

## BLE and WiFi Relationship

BLE and WiFi share radio/memory resources on ESP32.

This firmware uses the following policy:

- BLE on while provisioning or recovering connectivity
- BLE off during stable WiFi runtime
- BLE back on automatically if WiFi is lost

This avoids keeping BLE active during MCP runtime, which can starve the secure
WebSocket stack of heap.

## MCP Lifecycle

The MCP client is managed by the app layer rather than buried entirely inside
the integration wrapper.

The app is responsible for:

- deciding when MCP may start
- watching free heap
- waiting a grace period for connection
- scheduling retries if MCP does not come up

The `RobotMCP` class itself is intentionally thin:

- `begin()` starts the WebSocket client
- `loop()` pumps the connection
- `registerTools()` exposes robot capabilities once connected

## Persistence

Two different kinds of state are persisted in NVS.

### WiFi credentials

Stored by `wifi_manager.cpp`.

Used to reconnect automatically after reboot.

### Motor configuration

Stored by `config_nvs.cpp`.

Includes runtime-tuned values such as:

- PID P/I/D
- LPF Tf
- voltage limit
- velocity limit

These values are loaded after motor setup so saved tuning can override compile-
time defaults.

## Web UI

`robot_control.html` has two phases:

### Provisioning phase

- Connect to BLE
- Trigger WiFi scan
- Select SSID and send credentials
- Wait for `CONNECTED:ip|ssid|rssi|gw`

### Runtime control phase

- Switch to HTTP commands using robot IP
- Use D-pad/keyboard for direct movement
- Poll `/status` periodically for readout refresh

## Important Constraints

### Real-time behavior

Avoid long blocking calls in the main loop.
The motor loop must keep running continuously.

### Memory sensitivity

BLE + WiFi + HTTP + WSS on ESP32 is tight.
Changes that increase heap usage should be reviewed carefully.

### Shared control ownership

HTTP, serial, MCP, actions, and motion profiles all command the same robot.
Whenever a manual control path takes over, it should stop background scripted
behavior first.

### Secret handling

`MCP_ENDPOINT` currently contains a live token in source.
This should be treated as sensitive configuration and removed from public code.

## Recommended Future Improvements

1. Move MCP token out of source control
2. Add build verification after major refactors
3. Add small integration tests for HTTP command parsing and motion arbitration
4. Add auth or network restrictions for HTTP runtime control
5. Consider exporting shared module interfaces under `include/` if the codebase
   grows beyond the current single-firmware scope
