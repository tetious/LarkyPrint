#include <utility>

#include "map"
#include <ESPAsyncWebServer.h>
#include <AsyncWebSocket.h>

using namespace std::placeholders;
namespace WebSocket {
    struct OperationMessage {
        AsyncWebSocket *server;
        AsyncWebSocketClient *client;
        JsonObject &root;
    };

    struct BinaryMessage {
        AsyncWebSocket *server;
        AsyncWebSocketClient *client;
        uint8_t *payload;
        size_t payloadLength;
    };
}
using namespace WebSocket;

typedef function<void(OperationMessage)> OperationMessageCallback;
typedef function<void(BinaryMessage)> BinaryMessageCallback;

class WebSocketManager {

    WebSocketManager() : webSocket("/ws"), server(80) {
        webSocket.onEvent(std::bind(&WebSocketManager::webSocketEvent, this, _1, _2, _3, _4, _5, _6));
        server.addHandler(&webSocket);
        server.begin();
    }

    AsyncWebServer server;
    AsyncWebSocket webSocket;

    std::map<string, OperationMessageCallback> opcodeMap{};
    BinaryMessageCallback binaryHandler = nullptr;

    void
    webSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data,
                   size_t len) {
        static bool sdMode = true;
        switch (type) {
            case WS_EVT_DISCONNECT:
                Serial.printf("[%u] Disconnected!\r\n", client->id());
                break;
            case WS_EVT_CONNECT: {
                auto ip = client->remoteIP();
                Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\r\n", client->id(), ip[0], ip[1], ip[2],
                              ip[3], server->url());
            }
                break;
            case WS_EVT_DATA: {
                auto *info = static_cast<AwsFrameInfo *>(arg);
                if (info->final && info->index == 0 && info->len == len) {
                    Serial.println("Final.");
                }

                if (info->opcode == WS_TEXT) {
                    data[len] = 0;
                    Serial.printf("[%u] Got Text: %s\r\n", client->id(), data);
                    DynamicJsonBuffer jsonBuffer;
                    JsonObject &root = jsonBuffer.parseObject(data);
                    const char *op = root["op"];
                    if (opcodeMap.count(op) > 0) {
                        opcodeMap[op](OperationMessage{server, client, root});
                    }
                } else {
                    Serial.printf("[%u] get binary length: %u\r\n", client->id(), len);
                    if (binaryHandler) {
                        binaryHandler(BinaryMessage{server, client, data, len});
                    }
                }
                break;
            }
        }
    }


public:

    void onOp(const string &opCode, OperationMessageCallback callback) {
        opcodeMap[opCode] = std::move(callback);
    }

    void broadcast(const char *text) {
        webSocket.textAll(text);
    }

    void attachBinaryHandler(function<void(BinaryMessage)> callback) {
        if (binaryHandler != nullptr) {
            log_e("Called attachBinaryHandler while already attached!");
        } else {
            binaryHandler = std::move(callback);
        }
    }

    void detachBinaryHandler() {
        binaryHandler = nullptr;
    }

    WebSocketManager(WebSocketManager const &) = delete;

    WebSocketManager &operator=(WebSocketManager const &) = delete;

    static WebSocketManager &Instance() {
        static WebSocketManager i;
        return i;
    }
};