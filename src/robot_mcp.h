#pragma once

#include <WebSocketMCP.h>
#include <ArduinoJson.h>

class RobotMCP {
public:
    explicit RobotMCP(const char* endpoint);

    void begin();
    void loop();
    bool isConnected();

private:
    WebSocketMCP _client;
    String _endpoint;
    bool _toolRegistered = false;

    static RobotMCP* s_instance;
    static void _onConnection(bool ok);
    void onConnection(bool ok);
    void registerTools();

    static float parseSpeed(const String& args, float fallback, bool* ok = nullptr);
    static float clampSpeed(float spd);
    static WebSocketMCP::ToolResponse speedResp(const char* cmd, float spd);
    static WebSocketMCP::ToolResponse errorResp(const char* msg);
};
