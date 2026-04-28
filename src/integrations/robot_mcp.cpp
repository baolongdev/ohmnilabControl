#include "robot_mcp.h"
#include "../control/motion.h"
#include "../config.h"
#include "../storage/config_nvs.h"
#include "../control/motor_control.h"
#include "../control/robot_actions.h"
#include "../control/robot_motion_profiles.h"

#include <Arduino.h>
#include <WiFi.h>

RobotMCP* RobotMCP::s_instance = nullptr;

RobotMCP::RobotMCP(const char* endpoint) : _endpoint(endpoint ? endpoint : "") {
    s_instance = this;
}

// begin() can be called again after a failed or abandoned connection attempt.
// The app-level state machine is responsible for deciding when retries happen.
void RobotMCP::begin() {
    _client.begin(_endpoint.c_str(), RobotMCP::_onConnection);
}

void RobotMCP::loop() {
    _client.loop();
}

bool RobotMCP::isConnected() {
    return _client.isConnected();
}

void RobotMCP::_onConnection(bool ok) {
    if (s_instance) s_instance->onConnection(ok);
}

void RobotMCP::onConnection(bool ok) {
    if (ok) {
        Serial.println(F("[MCP] Connected - registering tools..."));
        registerTools();
    } else {
        Serial.println(F("[MCP] Disconnected."));
    }
}

static String configJson(const char* action) {
    String r = "{\"ok\":true";
    if (action) {
        r += ",\"action\":\"";
        r += action;
        r += "\"";
    }
    r += ",\"persisted\":";
    r += configNvsHasSaved() ? "true" : "false";
    r += ",\"pid\":{\"p\":";
    r += String(motor.PID_velocity.P, 6);
    r += ",\"i\":";
    r += String(motor.PID_velocity.I, 6);
    r += ",\"d\":";
    r += String(motor.PID_velocity.D, 6);
    r += "}";
    r += ",\"lpf_tf\":";
    r += String(motor.LPF_velocity.Tf, 6);
    r += ",\"voltage_limit\":";
    r += String(motor.voltage_limit, 3);
    r += ",\"velocity_limit\":";
    r += String(motor.velocity_limit, 3);
    r += ",\"target_left\":";
    r += String(target_left, 3);
    r += ",\"target_right\":";
    r += String(target_right, 3);
    r += "}";
    return r;
}

static bool parseObject(const String& args, DynamicJsonDocument& doc) {
    if (args.length() == 0) return true;
    DeserializationError err = deserializeJson(doc, args);
    return !err && doc.is<JsonObject>();
}

static bool validPositive(float value, float minValue, float maxValue) {
    return isfinite(value) && value >= minValue && value <= maxValue;
}

static float jsonFloatOr(const JsonDocument& doc, const char* key, float fallback) {
    return doc.containsKey(key) ? doc[key].as<float>() : fallback;
}

static unsigned long jsonMsOr(const JsonDocument& doc, const char* key, unsigned long fallback) {
    return doc.containsKey(key) ? doc[key].as<unsigned long>() : fallback;
}

void RobotMCP::registerTools() {
    if (_toolRegistered) return;
    _toolRegistered = true;

    // Tool registration happens once per process lifetime after the first
    // successful MCP connection. The callbacks themselves stay lightweight and
    // dispatch into the local control modules.

    _client.registerTool(
        "move_forward",
        "Di chuyen robot tien thang. speed tinh bang rad/s.",
        R"({"type":"object","properties":{"speed":{"type":"number","description":"Van toc (rad/s), range 5-velocity_limit","default":20}}})",
        [](const String& args) -> WebSocketMCP::ToolResponse {
            bool ok = true;
            float spd = parseSpeed(args, 20.0f, &ok);
            if (!ok) return errorResp("invalid_args");
            actionStop();
            motionProfileStop();
            Forward(spd);
            return speedResp("forward", spd);
        }
    );

    _client.registerTool(
        "move_backward",
        "Di chuyen robot lui thang. speed tinh bang rad/s.",
        R"({"type":"object","properties":{"speed":{"type":"number","description":"Van toc (rad/s), range 5-velocity_limit","default":20}}})",
        [](const String& args) -> WebSocketMCP::ToolResponse {
            bool ok = true;
            float spd = parseSpeed(args, 20.0f, &ok);
            if (!ok) return errorResp("invalid_args");
            actionStop();
            motionProfileStop();
            Backward(spd);
            return speedResp("backward", spd);
        }
    );

    _client.registerTool(
        "turn_left",
        "Quay robot sang trai tai cho (2 banh quay nguoc nhau).",
        R"({"type":"object","properties":{"speed":{"type":"number","description":"Van toc quay (rad/s), range 5-velocity_limit","default":15}}})",
        [](const String& args) -> WebSocketMCP::ToolResponse {
            bool ok = true;
            float spd = parseSpeed(args, 15.0f, &ok);
            if (!ok) return errorResp("invalid_args");
            actionStop();
            motionProfileStop();
            TurnLeft(spd);
            return speedResp("turn_left", spd);
        }
    );

    _client.registerTool(
        "turn_right",
        "Quay robot sang phai tai cho (2 banh quay nguoc nhau).",
        R"({"type":"object","properties":{"speed":{"type":"number","description":"Van toc quay (rad/s), range 5-velocity_limit","default":15}}})",
        [](const String& args) -> WebSocketMCP::ToolResponse {
            bool ok = true;
            float spd = parseSpeed(args, 15.0f, &ok);
            if (!ok) return errorResp("invalid_args");
            actionStop();
            motionProfileStop();
            TurnRight(spd);
            return speedResp("turn_right", spd);
        }
    );

    _client.registerTool(
        "stop",
        "Dung robot ngay lap tuc (ca 2 motor ve 0).",
        R"({"type":"object","properties":{}})",
        [](const String&) -> WebSocketMCP::ToolResponse {
            actionStop();
            motionProfileStop();
            return WebSocketMCP::ToolResponse("{\"ok\":true,\"cmd\":\"stop\"}");
        }
    );

    _client.registerTool(
        "get_status",
        "Lay trang thai hien tai cua robot.",
        R"({"type":"object","properties":{}})",
        [](const String&) -> WebSocketMCP::ToolResponse {
            String r = "{\"ok\":true,\"target_left\":";
            r += String(target_left, 3);
            r += ",\"target_right\":";
            r += String(target_right, 3);
            r += ",\"velocity_limit\":";
            r += String(motorRuntimeVelocityLimit(), 3);
            r += ",\"wifi\":";
            r += (WiFi.status() == WL_CONNECTED ? "true" : "false");
            r += ",\"ip\":\"";
            r += WiFi.localIP().toString();
            r += "\"}";
            return WebSocketMCP::ToolResponse(r);
        }
    );

    _client.registerTool(
        "get_config",
        "Doc cau hinh motor hien tai: PID, LPF, voltage_limit, velocity_limit.",
        R"({"type":"object","properties":{}})",
        [](const String&) -> WebSocketMCP::ToolResponse {
            return WebSocketMCP::ToolResponse(configJson("get_config"));
        }
    );

    _client.registerTool(
        "set_pid",
        "Cap nhat PID velocity cho ca 2 motor. Tham so p/i/d tuy chon, save=true de luu NVS.",
        R"({"type":"object","properties":{"p":{"type":"number","description":"PID P, 0-10"},"i":{"type":"number","description":"PID I, 0-100"},"d":{"type":"number","description":"PID D, 0-10"},"save":{"type":"boolean","default":false}}})",
        [](const String& args) -> WebSocketMCP::ToolResponse {
            DynamicJsonDocument doc(256);
            if (!parseObject(args, doc)) return errorResp("invalid_args");
            float p = doc.containsKey("p") ? doc["p"].as<float>() : motor.PID_velocity.P;
            float i = doc.containsKey("i") ? doc["i"].as<float>() : motor.PID_velocity.I;
            float d = doc.containsKey("d") ? doc["d"].as<float>() : motor.PID_velocity.D;
            bool save = doc["save"] | false;
            if (!validPositive(p, 0.0f, 10.0f) || !validPositive(i, 0.0f, 100.0f) || !validPositive(d, 0.0f, 10.0f)) {
                return errorResp("pid_out_of_range");
            }
            motorApplyConfig(p, i, d, motor.LPF_velocity.Tf, motor.voltage_limit, motor.velocity_limit);
            if (save) configNvsSave();
            return WebSocketMCP::ToolResponse(configJson(save ? "set_pid_saved" : "set_pid"));
        }
    );

    _client.registerTool(
        "set_limits",
        "Cap nhat voltage_limit va/hoac velocity_limit cho ca 2 motor. save=true de luu NVS.",
        R"({"type":"object","properties":{"voltage_limit":{"type":"number","description":"Gioi han dien ap motor, 1-17.5 V"},"velocity_limit":{"type":"number","description":"Gioi han toc do, 5-80 rad/s"},"lpf_tf":{"type":"number","description":"Velocity LPF Tf, 0.001-1.0 s"},"save":{"type":"boolean","default":false}}})",
        [](const String& args) -> WebSocketMCP::ToolResponse {
            DynamicJsonDocument doc(256);
            if (!parseObject(args, doc)) return errorResp("invalid_args");
            float voltLimit = doc.containsKey("voltage_limit") ? doc["voltage_limit"].as<float>() : motor.voltage_limit;
            float velLimit = doc.containsKey("velocity_limit") ? doc["velocity_limit"].as<float>() : motor.velocity_limit;
            float lpfTf = doc.containsKey("lpf_tf") ? doc["lpf_tf"].as<float>() : motor.LPF_velocity.Tf;
            bool save = doc["save"] | false;
            if (!validPositive(voltLimit, 1.0f, voltage_power_supply) ||
                !validPositive(velLimit, DEADZONE, 80.0f) ||
                !validPositive(lpfTf, 0.001f, 1.0f)) {
                return errorResp("limits_out_of_range");
            }
            motorApplyConfig(motor.PID_velocity.P, motor.PID_velocity.I, motor.PID_velocity.D,
                             lpfTf, voltLimit, velLimit);
            if (target_left > velLimit) target_left = velLimit;
            if (target_left < -velLimit) target_left = -velLimit;
            if (target_right > velLimit) target_right = velLimit;
            if (target_right < -velLimit) target_right = -velLimit;
            if (save) configNvsSave();
            return WebSocketMCP::ToolResponse(configJson(save ? "set_limits_saved" : "set_limits"));
        }
    );

    _client.registerTool(
        "save_config",
        "Luu cau hinh motor hien tai vao NVS.",
        R"({"type":"object","properties":{}})",
        [](const String&) -> WebSocketMCP::ToolResponse {
            configNvsSave();
            return WebSocketMCP::ToolResponse(configJson("save_config"));
        }
    );

    _client.registerTool(
        "load_config",
        "Nap lai cau hinh motor tu NVS; neu chua co thi dung defaults trong firmware.",
        R"({"type":"object","properties":{}})",
        [](const String&) -> WebSocketMCP::ToolResponse {
            configNvsLoad();
            return WebSocketMCP::ToolResponse(configJson("load_config"));
        }
    );

    _client.registerTool(
        "reset_config",
        "Xoa cau hinh motor trong NVS va ap lai defaults compile-time.",
        R"({"type":"object","properties":{}})",
        [](const String&) -> WebSocketMCP::ToolResponse {
            actionStop();
            motionProfileStop();
            configNvsClear();
            return WebSocketMCP::ToolResponse(configJson("reset_config"));
        }
    );

    _client.registerTool(
        "move_timed",
        "Chay theo huong va thoi gian xac dinh. Non-blocking; dung get_motion_status de theo doi.",
        R"({"type":"object","properties":{"direction":{"type":"string","description":"forward | backward | turn_left | turn_right","default":"forward"},"speed":{"type":"number","description":"Toc do rad/s","default":15},"duration_ms":{"type":"integer","description":"Thoi gian chay, range 100-60000 ms","default":1000}}})",
        [](const String& args) -> WebSocketMCP::ToolResponse {
            DynamicJsonDocument doc(192);
            if (!parseObject(args, doc)) return errorResp("invalid_args");
            String direction = doc["direction"] | "forward";
            float speed = jsonFloatOr(doc, "speed", 15.0f);
            unsigned long durationMs = jsonMsOr(doc, "duration_ms", 1000);
            actionStop();
            motionProfileStop();
            if (!motionProfileStartTimed(direction, speed, durationMs)) return errorResp("invalid_motion_profile");
            return WebSocketMCP::ToolResponse(motionProfileStatusJson());
        }
    );

    _client.registerTool(
        "move_distance",
        "Chay tien hoac lui theo quang duong mong muon. Non-blocking; dung get_motion_status de theo doi.",
        R"({"type":"object","properties":{"direction":{"type":"string","description":"forward | backward","default":"forward"},"speed":{"type":"number","description":"Toc do rad/s","default":20},"distance_m":{"type":"number","description":"Quang duong can di chuyen, don vi met","default":0.5}}})",
        [](const String& args) -> WebSocketMCP::ToolResponse {
            DynamicJsonDocument doc(192);
            if (!parseObject(args, doc)) return errorResp("invalid_args");
            String direction = doc["direction"] | "forward";
            float speed = jsonFloatOr(doc, "speed", 20.0f);
            float distanceM = jsonFloatOr(doc, "distance_m", 0.5f);
            actionStop();
            motionProfileStop();
            if (!motionProfileStartDistance(direction, speed, distanceM)) return errorResp("invalid_motion_profile");
            return WebSocketMCP::ToolResponse(motionProfileStatusJson());
        }
    );

    _client.registerTool(
        "turn_angle",
        "Quay robot sang trai hoac phai theo goc mong muon. Non-blocking; dung get_motion_status de theo doi.",
        R"({"type":"object","properties":{"direction":{"type":"string","description":"left | right","default":"left"},"speed":{"type":"number","description":"Toc do quay rad/s","default":15},"angle_deg":{"type":"number","description":"Goc can quay, don vi do","default":90}}})",
        [](const String& args) -> WebSocketMCP::ToolResponse {
            DynamicJsonDocument doc(192);
            if (!parseObject(args, doc)) return errorResp("invalid_args");
            String direction = doc["direction"] | "left";
            float speed = jsonFloatOr(doc, "speed", 15.0f);
            float angleDeg = jsonFloatOr(doc, "angle_deg", 90.0f);
            actionStop();
            motionProfileStop();
            if (!motionProfileStartTurnAngle(direction, speed, angleDeg)) return errorResp("invalid_motion_profile");
            return WebSocketMCP::ToolResponse(motionProfileStatusJson());
        }
    );

    _client.registerTool(
        "get_motion_status",
        "Lay trang thai motion profile dang chay.",
        R"({"type":"object","properties":{}})",
        [](const String&) -> WebSocketMCP::ToolResponse {
            return WebSocketMCP::ToolResponse(motionProfileStatusJson());
        }
    );

    _client.registerTool(
        "stop_motion",
        "Dung motion profile dang chay va stop robot.",
        R"({"type":"object","properties":{}})",
        [](const String&) -> WebSocketMCP::ToolResponse {
            motionProfileStop();
            return WebSocketMCP::ToolResponse(motionProfileStatusJson());
        }
    );

    _client.registerTool(
        "ACTION1",
        "ACTION1: quay trai, dung ngan, quay phai, dung. Non-blocking.",
        R"({"type":"object","properties":{"speed":{"type":"number","description":"Toc do quay rad/s","default":15},"step_ms":{"type":"integer","description":"Thoi gian moi pha quay, 200-5000 ms","default":900}}})",
        [](const String& args) -> WebSocketMCP::ToolResponse {
            DynamicJsonDocument doc(160);
            if (!parseObject(args, doc)) return errorResp("invalid_args");
            float speed = jsonFloatOr(doc, "speed", 15.0f);
            unsigned long stepMs = jsonMsOr(doc, "step_ms", 900);
            motionProfileStop();
            actionStartAction1(speed, stepMs);
            return WebSocketMCP::ToolResponse(actionStatusJson());
        }
    );

    _client.registerTool(
        "ACTION2",
        "ACTION2: tien, dung ngan, lui, dung. Non-blocking.",
        R"({"type":"object","properties":{"speed":{"type":"number","description":"Toc do tien/lui rad/s","default":18},"step_ms":{"type":"integer","description":"Thoi gian moi pha tien/lui, 200-5000 ms","default":1000}}})",
        [](const String& args) -> WebSocketMCP::ToolResponse {
            DynamicJsonDocument doc(160);
            if (!parseObject(args, doc)) return errorResp("invalid_args");
            float speed = jsonFloatOr(doc, "speed", 18.0f);
            unsigned long stepMs = jsonMsOr(doc, "step_ms", 1000);
            motionProfileStop();
            actionStartAction2(speed, stepMs);
            return WebSocketMCP::ToolResponse(actionStatusJson());
        }
    );

    _client.registerTool(
        "ACTION3",
        "ACTION3: zigzag ngan voi cac nhhip tien va doi huong xen ke. Non-blocking.",
        R"({"type":"object","properties":{"speed":{"type":"number","description":"Toc do rad/s","default":15},"step_ms":{"type":"integer","description":"Thoi gian co ban moi nhip, 200-5000 ms","default":700}}})",
        [](const String& args) -> WebSocketMCP::ToolResponse {
            DynamicJsonDocument doc(160);
            if (!parseObject(args, doc)) return errorResp("invalid_args");
            float speed = jsonFloatOr(doc, "speed", 15.0f);
            unsigned long stepMs = jsonMsOr(doc, "step_ms", 700);
            motionProfileStop();
            actionStartAction3(speed, stepMs);
            return WebSocketMCP::ToolResponse(actionStatusJson());
        }
    );

    _client.registerTool(
        "ACTION4",
        "ACTION4: quet trai-phai kieu scan sweep. Non-blocking.",
        R"({"type":"object","properties":{"speed":{"type":"number","description":"Toc do quay rad/s","default":16},"step_ms":{"type":"integer","description":"Thoi gian quet co ban, 200-5000 ms","default":600}}})",
        [](const String& args) -> WebSocketMCP::ToolResponse {
            DynamicJsonDocument doc(160);
            if (!parseObject(args, doc)) return errorResp("invalid_args");
            float speed = jsonFloatOr(doc, "speed", 16.0f);
            unsigned long stepMs = jsonMsOr(doc, "step_ms", 600);
            motionProfileStop();
            actionStartAction4(speed, stepMs);
            return WebSocketMCP::ToolResponse(actionStatusJson());
        }
    );

    _client.registerTool(
        "ACTION5",
        "ACTION5: spin burst nhanh de demo phan ung quay. Non-blocking.",
        R"({"type":"object","properties":{"speed":{"type":"number","description":"Toc do quay rad/s","default":14},"step_ms":{"type":"integer","description":"Thoi gian co ban, 200-5000 ms","default":500}}})",
        [](const String& args) -> WebSocketMCP::ToolResponse {
            DynamicJsonDocument doc(160);
            if (!parseObject(args, doc)) return errorResp("invalid_args");
            float speed = jsonFloatOr(doc, "speed", 14.0f);
            unsigned long stepMs = jsonMsOr(doc, "step_ms", 500);
            motionProfileStop();
            actionStartAction5(speed, stepMs);
            return WebSocketMCP::ToolResponse(actionStatusJson());
        }
    );

    _client.registerTool(
        "ACTION6",
        "ACTION6: patrol ngan gom tien, doi huong, lui nhe roi dung. Non-blocking.",
        R"({"type":"object","properties":{"speed":{"type":"number","description":"Toc do rad/s","default":16},"step_ms":{"type":"integer","description":"Thoi gian co ban moi nhip, 200-5000 ms","default":650}}})",
        [](const String& args) -> WebSocketMCP::ToolResponse {
            DynamicJsonDocument doc(160);
            if (!parseObject(args, doc)) return errorResp("invalid_args");
            float speed = jsonFloatOr(doc, "speed", 16.0f);
            unsigned long stepMs = jsonMsOr(doc, "step_ms", 650);
            motionProfileStop();
            actionStartAction6(speed, stepMs);
            return WebSocketMCP::ToolResponse(actionStatusJson());
        }
    );

    _client.registerTool(
        "emotion_disagree",
        "Lac trai-phai lien tuc de the hien su khong dong y. Non-blocking.",
        R"({"type":"object","properties":{"speed":{"type":"number","description":"Toc do quay rad/s","default":15},"step_ms":{"type":"integer","description":"Do dai moi nhip lac, 200-5000 ms","default":450}}})",
        [](const String& args) -> WebSocketMCP::ToolResponse {
            DynamicJsonDocument doc(160);
            if (!parseObject(args, doc)) return errorResp("invalid_args");
            float speed = jsonFloatOr(doc, "speed", 15.0f);
            unsigned long stepMs = jsonMsOr(doc, "step_ms", 450);
            motionProfileStop();
            actionStartEmotionDisagree(speed, stepMs);
            return WebSocketMCP::ToolResponse(actionStatusJson());
        }
    );

    _client.registerTool(
        "emotion_happy",
        "Quay vong vong de the hien trang thai vui ve. Non-blocking.",
        R"({"type":"object","properties":{"speed":{"type":"number","description":"Toc do quay rad/s","default":16},"step_ms":{"type":"integer","description":"Do dai moi nhip quay, 200-5000 ms","default":700}}})",
        [](const String& args) -> WebSocketMCP::ToolResponse {
            DynamicJsonDocument doc(160);
            if (!parseObject(args, doc)) return errorResp("invalid_args");
            float speed = jsonFloatOr(doc, "speed", 16.0f);
            unsigned long stepMs = jsonMsOr(doc, "step_ms", 700);
            motionProfileStop();
            actionStartEmotionHappy(speed, stepMs);
            return WebSocketMCP::ToolResponse(actionStatusJson());
        }
    );

    _client.registerTool(
        "emotion_curious",
        "Quet trai-phai kieu ngoc nghenh, nhu dang nhin quanh. Non-blocking.",
        R"({"type":"object","properties":{"speed":{"type":"number","description":"Toc do quay rad/s","default":15},"step_ms":{"type":"integer","description":"Do dai moi nhip quet, 200-5000 ms","default":550}}})",
        [](const String& args) -> WebSocketMCP::ToolResponse {
            DynamicJsonDocument doc(160);
            if (!parseObject(args, doc)) return errorResp("invalid_args");
            float speed = jsonFloatOr(doc, "speed", 15.0f);
            unsigned long stepMs = jsonMsOr(doc, "step_ms", 550);
            motionProfileStop();
            actionStartEmotionCurious(speed, stepMs);
            return WebSocketMCP::ToolResponse(actionStatusJson());
        }
    );

    _client.registerTool(
        "emotion_excited",
        "Chay nhun nhay va doi huong nhanh de the hien su phan khich. Non-blocking.",
        R"({"type":"object","properties":{"speed":{"type":"number","description":"Toc do rad/s","default":17},"step_ms":{"type":"integer","description":"Do dai moi nhip, 200-5000 ms","default":450}}})",
        [](const String& args) -> WebSocketMCP::ToolResponse {
            DynamicJsonDocument doc(160);
            if (!parseObject(args, doc)) return errorResp("invalid_args");
            float speed = jsonFloatOr(doc, "speed", 17.0f);
            unsigned long stepMs = jsonMsOr(doc, "step_ms", 450);
            motionProfileStop();
            actionStartEmotionExcited(speed, stepMs);
            return WebSocketMCP::ToolResponse(actionStatusJson());
        }
    );

    _client.registerTool(
        "emotion_shy",
        "Rut nhe ra sau, nghieng minh mot chut roi quay lai nhe nhang. Non-blocking.",
        R"({"type":"object","properties":{"speed":{"type":"number","description":"Toc do rad/s","default":12},"step_ms":{"type":"integer","description":"Do dai moi nhip, 200-5000 ms","default":500}}})",
        [](const String& args) -> WebSocketMCP::ToolResponse {
            DynamicJsonDocument doc(160);
            if (!parseObject(args, doc)) return errorResp("invalid_args");
            float speed = jsonFloatOr(doc, "speed", 12.0f);
            unsigned long stepMs = jsonMsOr(doc, "step_ms", 500);
            motionProfileStop();
            actionStartEmotionShy(speed, stepMs);
            return WebSocketMCP::ToolResponse(actionStatusJson());
        }
    );

    _client.registerTool(
        "action_random_move",
        "Action random: tu chon tien/lui/quay trai/quay phai trong khoang 2-3 giay. Non-blocking.",
        R"({"type":"object","properties":{"speed":{"type":"number","description":"Toc do rad/s","default":15},"min_ms":{"type":"integer","description":"Thoi gian toi thieu, default 2000","default":2000},"max_ms":{"type":"integer","description":"Thoi gian toi da, default 3000","default":3000}}})",
        [](const String& args) -> WebSocketMCP::ToolResponse {
            DynamicJsonDocument doc(192);
            if (!parseObject(args, doc)) return errorResp("invalid_args");
            float speed = jsonFloatOr(doc, "speed", 15.0f);
            unsigned long minMs = jsonMsOr(doc, "min_ms", 2000);
            unsigned long maxMs = jsonMsOr(doc, "max_ms", 3000);
            motionProfileStop();
            actionStartRandomMove(speed, minMs, maxMs);
            return WebSocketMCP::ToolResponse(actionStatusJson());
        }
    );

    _client.registerTool(
        "get_action_status",
        "Lay trang thai action preset dang chay.",
        R"({"type":"object","properties":{}})",
        [](const String&) -> WebSocketMCP::ToolResponse {
            return WebSocketMCP::ToolResponse(actionStatusJson());
        }
    );

    _client.registerTool(
        "stop_action",
        "Dung action preset dang chay va stop robot.",
        R"({"type":"object","properties":{}})",
        [](const String&) -> WebSocketMCP::ToolResponse {
            actionStop();
            return WebSocketMCP::ToolResponse(actionStatusJson());
        }
    );

    Serial.println(F("[MCP] 31 tools registered: move_forward, move_backward, turn_left, turn_right, stop, get_status, get_config, set_pid, set_limits, save_config, load_config, reset_config, move_timed, move_distance, turn_angle, get_motion_status, stop_motion, ACTION1, ACTION2, ACTION3, ACTION4, ACTION5, ACTION6, emotion_disagree, emotion_happy, emotion_curious, emotion_excited, emotion_shy, action_random_move, get_action_status, stop_action"));
}

float RobotMCP::parseSpeed(const String& args, float fallback, bool* ok) {
    if (ok) *ok = true;

    if (args.length() == 0) {
        return clampSpeed(fallback);
    }

    DynamicJsonDocument doc(96);
    DeserializationError err = deserializeJson(doc, args);
    if (err || !doc.is<JsonObject>()) {
        if (ok) *ok = false;
        return 0.0f;
    }

    float spd = doc["speed"] | fallback;
    return clampSpeed(spd);
}

float RobotMCP::clampSpeed(float spd) {
    if (spd < 0) spd = -spd;
    float limit = motorRuntimeVelocityLimit();
    if (spd > limit) spd = limit;
    return spd;
}

WebSocketMCP::ToolResponse RobotMCP::speedResp(const char* cmd, float spd) {
    String r = "{\"ok\":true,\"cmd\":\"";
    r += cmd;
    r += "\",\"speed\":";
    r += String(spd, 3);
    r += "}";
    return WebSocketMCP::ToolResponse(r);
}

WebSocketMCP::ToolResponse RobotMCP::errorResp(const char* msg) {
    String r = "{\"ok\":false,\"error\":\"";
    r += msg;
    r += "\"}";
    return WebSocketMCP::ToolResponse(r, true);
}
