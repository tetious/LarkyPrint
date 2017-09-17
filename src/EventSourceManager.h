#include "Arduino.h"
#include <ESPAsyncWebServer.h>

class EventSourceManager {
    AsyncWebServer &server;
    AsyncEventSource eventSource;

public:

    explicit EventSourceManager(AsyncWebServer &webServer) : eventSource("/events"), server(webServer) {
        server.addHandler(&eventSource);
        eventSource.onConnect([](AsyncEventSourceClient *client) {
            auto ip = client->client()->remoteIP();
            Serial.printf("EventStream::Connected from %d.%d.%d.%d\r\n", ip[0], ip[1], ip[2], ip[3]);
        });
    }

    void broadcast(const char *text, const char *event) {
        eventSource.send(text, event);
    }
};