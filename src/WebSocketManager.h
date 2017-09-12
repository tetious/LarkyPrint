#include "map"
#include "WebSocketsServer.h"

using namespace std::placeholders;
namespace WebSocket {
    struct OperationMessage {
        JsonObject &root;
        uint8_t connectionNumber;
    };

    struct BinaryMessage {
        uint8_t connectionNumber;
        uint8_t *payload;
        size_t payloadLength;
    };
}
using namespace WebSocket;

class WebSocketManager {

    WebSocketManager() : webSocket(80) {
        webSocket.onEvent(std::bind(&WebSocketManager::webSocketEvent, this, _1, _2, _3, _4));
        webSocket.begin();
    }

    WebSocketsServer webSocket;
    std::map<string, function<void(OperationMessage)>> opcodeMap{};
    function<void(BinaryMessage)> binaryHandler = nullptr;

    void webSocketEvent(uint8_t connectionNumber, WStype_t type, uint8_t *payload, size_t length) {
        static bool sdMode = true;
        switch (type) {
            case WStype_DISCONNECTED:
                Serial.printf("[%u] Disconnected!\r\n", connectionNumber);
                break;
            case WStype_CONNECTED: {
                IPAddress ip = webSocket.remoteIP(connectionNumber);
                Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\r\n", connectionNumber, ip[0], ip[1], ip[2],
                              ip[3], payload);
            }
                break;
            case WStype_TEXT: {
                Serial.printf("[%u] Got Text: %s\r\n", connectionNumber, payload);
                DynamicJsonBuffer jsonBuffer;
                JsonObject &root = jsonBuffer.parseObject(payload);
                const char *op = root["op"];
                if (opcodeMap.count(op) > 0) {
                    opcodeMap[op](OperationMessage{root, connectionNumber});
                }
                break;
            }
            case WStype_BIN: {
                Serial.printf("[%u] get binary length: %u\r\n", connectionNumber, length);
                if (binaryHandler) {
                    binaryHandler(BinaryMessage{connectionNumber, payload, length});
                }
                break;
            }
        }
    }


public:

    void onOp(const string &opCode, function<void(OperationMessage)> callback) {
        opcodeMap[opCode] = callback;
    }

    void broadcast(const string &text) {
        webSocket.broadcastTXT(text.c_str());
    }

    void sendText(uint8_t connectionNumber, const char *text) {
        webSocket.sendTXT(connectionNumber, text);
    }

    void loop() {
        webSocket.loop(); // todo: use async
    }

    void attachBinaryHandler(function<void(BinaryMessage)> callback) {
        if (binaryHandler != nullptr) {
            log_e("Called attachBinaryHandler while already attached!");
        } else {
            binaryHandler = callback;
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