# OhmniRobot Control

Firmware and control assets for an ESP32-based 2-wheel OhmniRobot platform.
The project uses SimpleFOC for BLDC motor control, BLE for WiFi provisioning,
HTTP for local runtime control, and MCP over secure WebSocket for higher-level
tool-driven integration.

For a deeper explanation of module boundaries and runtime flow, see
[`ARCHITECTURE.md`](./ARCHITECTURE.md).

## Features

- Dual BLDC wheel control with SimpleFOC
- BLE WiFi provisioning and recovery flow
- Local HTTP runtime control endpoints
- Browser-based control page via `robot_control.html`
- MCP integration over WebSocket
- Runtime-adjustable motor config with NVS persistence
- Non-blocking motion profiles and scripted robot actions

## Hardware Assumptions

- ESP32 Dev Module
- 2x BLDC motors
- 2x AS5600 magnetic sensors
- 2x 3PWM motor drivers

Current default pin mapping is defined in `src/config.h` and reflected in
`ohmni_robot_mcp.yaml`.

## Project Layout

- `src/app` - application state machine and main loop
- `src/control` - motor control, motion primitives, action presets, motion profiles
- `src/connectivity` - BLE provisioning and WiFi helpers
- `src/interface` - HTTP server and serial command interface
- `src/integrations` - MCP integration
- `src/storage` - NVS-backed persistence helpers
- `src/config.h` - compile-time configuration
- `robot_control.html` - browser UI for provisioning and direct HTTP control
- `ARCHITECTURE.md` - high-level architecture and runtime flow notes

## Requirements

- [PlatformIO](https://platformio.org/)
- ESP32 board support
- USB connection to the target board
- Chrome or Edge for Web Bluetooth provisioning UI

## Build and Flash

The default PlatformIO environment is `esp32dev`.

Build:

```bash
pio run
```

Upload:

```bash
pio run -t upload
```

Serial monitor:

```bash
pio device monitor
```

The current `platformio.ini` is configured with a fixed serial port. If your
board appears on another port, update:

- `monitor_port`
- `upload_port`

## First-Time Setup Flow

## 1. Flash the firmware

Upload the firmware to the ESP32.

## 2. Open the control page

Open `robot_control.html` in a browser that supports Web Bluetooth.

Recommended:

- Chrome
- Edge

Web Bluetooth requires a secure context, typically:

- `https://...`
- `http://localhost`

## 3. Connect over BLE

Use the page button to connect to the device named:

- `OhmniRobot`

## 4. Scan/select WiFi

Use BLE provisioning to:

- scan nearby WiFi networks
- choose an SSID
- send SSID/password to the robot

## 5. Robot joins WiFi

Once connected, the firmware sends connection info back over BLE, including:

- IP address
- SSID
- RSSI
- gateway

After that, BLE is disabled during stable runtime to free heap for HTTP + MCP.
If WiFi is lost later, BLE provisioning is started again automatically.

## Runtime Control

## HTTP endpoints

The firmware exposes a small HTTP control surface on port `80`.

Examples:

- `GET /forward?speed=20`
- `GET /backward?speed=20`
- `GET /left?speed=15`
- `GET /right?speed=15`
- `GET /stop`
- `GET /status`

Example:

```text
http://<ROBOT_IP>/forward?speed=20
```

The web UI uses these endpoints directly after provisioning is complete.

## Serial Commands

Useful serial commands include:

- `F20` - forward at 20 rad/s
- `B15` - backward at 15 rad/s
- `L10` - turn left at 10 rad/s
- `R10` - turn right at 10 rad/s
- `S` - stop
- `A1`..`A6` - action presets
- `AR` - random action
- `AS` - stop action
- `STATUS` or `?` - print full status
- `HELP` or `H` - print help
- `CLEAR` - clear saved WiFi credentials and reboot into BLE provisioning

## Motion Model

The control stack is layered:

1. Direct wheel target commands in `src/control/motion.cpp`
2. Scripted robot actions in `src/control/robot_actions.cpp`
3. Goal-oriented motion profiles in `src/control/robot_motion_profiles.cpp`

All control surfaces eventually converge on the same left/right target wheel
velocity variables.

## MCP Integration

The project includes MCP integration via `src/integrations/robot_mcp.cpp`.
Tools cover:

- direct movement
- stop/status
- motor tuning
- motion profiles
- action presets

The app starts MCP only after WiFi is up and BLE has been shut down to recover
heap for secure WebSocket usage.

If MCP does not connect immediately, the firmware waits a grace period and then
retries automatically.

## Configuration

Main compile-time settings live in `src/config.h`, including:

- motor defaults
- deadzone and stop-zone behavior
- wheel geometry
- BLE UUIDs
- WiFi timeout
- MCP retry timing
- MCP endpoint

Motor runtime settings can also be persisted in NVS using the config tools and
load on boot.

## Important Notes

## BLE vs WiFi vs MCP memory tradeoff

ESP32 memory is limited when using:

- BLE
- WiFi
- HTTP server
- secure WebSocket (WSS)

This firmware chooses the following runtime policy:

- BLE enabled for provisioning/recovery
- BLE disabled during stable WiFi runtime
- BLE automatically restored if WiFi is lost

## Security note

`src/config.h` currently contains an MCP endpoint token directly in source.
If this repository is public, treat that token as exposed and rotate/remove it.

## Related Files

- `ARCHITECTURE.md` - deeper explanation of the runtime architecture
- `platformio.ini` - PlatformIO environment settings
- `ohmni_robot_mcp.yaml` - MCP manifest
- `robot_control.html` - browser UI

## Suggested Next Steps

1. Build the firmware with `pio run`
2. Flash the board and verify BLE provisioning
3. Test WiFi join and HTTP control page
4. Verify MCP connection in serial logs
5. Move the MCP token out of source control
