#include "Arduino.h"
#include "hd44780.h"
#include "RingBuffer.h"
#include <WiFi.h>
#include <WebSocketsServer.h>
#include "secure.h"

const byte lcd_pins[] = {25, 26, 12, 13};
const byte lcd_clk = 27;
const byte lcd_rs = 14;

WebSocketsServer webSocket = WebSocketsServer(80);

struct lcd_cap {
    volatile bool cmd;
    volatile char data;
};

RingBuffer<lcd_cap> buffer;

IRAM_ATTR void read() {
    static bool firstHalf = true;
    static lcd_cap current = lcd_cap();

    // for atomicity and efficiency, grab the whole GPIO at once.
    auto read = GPIO.in;

    if (bitRead(read, lcd_rs) == 0) {
        current.cmd = true;
    }

    // full byte is broken into two, so stuff them into the correct place
    for (int pin = 0; pin < sizeof(lcd_pins); pin++) {
        current.data |= (bitRead(read, lcd_pins[pin]) << pin + (firstHalf ? 4 : 0));
    }

    if (!firstHalf) {
        buffer.write(current);
        current = lcd_cap();
    }

    firstHalf = !firstHalf;
}

void processItem() {
    auto cap = buffer.read();
    if (cap == nullptr) return;
    if (cap->cmd) {
        if ((cap->data & LCD_SETDDRAMADDR) == LCD_SETDDRAMADDR) {
            auto loc = static_cast<byte>(cap->data & 0x7F); // grab last 7 bits
            auto row = static_cast<byte>(loc / 20), col = static_cast<byte>(loc % 20);
            if (loc == 0) {
                Serial.println();
            }
        }
    } else {
        Serial.write(cap->data);
    }
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {

    switch(type) {
        case WStype_DISCONNECTED:
            Serial.printf("[%u] Disconnected!\n", num);
            break;
        case WStype_CONNECTED:
        {
            IPAddress ip = webSocket.remoteIP(num);
            Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);

            // send message to client
            webSocket.sendTXT(num, "Connected");
        }
            break;
        case WStype_TEXT:
            Serial.printf("[%u] get Text: %s\n", num, payload);

            // send message to client
            // webSocket.sendTXT(num, "message here");

            // send data to all connected clients
            // webSocket.broadcastTXT("message here");
            break;
        case WStype_BIN:
            Serial.printf("[%u] get binary length: %u\n", num, length);
            //hexdump(payload, length);

            // send message to client
            // webSocket.sendBIN(num, payload, length);
            break;
    }
}

void setup() {
    Serial.begin(115200);
    Serial.setDebugOutput(true);
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());

    for (unsigned char lcd_pin : lcd_pins) {
        pinMode(lcd_pin, INPUT_PULLDOWN);
    }
    pinMode(lcd_clk, INPUT_PULLDOWN);
    pinMode(lcd_rs, INPUT_PULLDOWN);
    attachInterrupt(lcd_clk, read, FALLING);

    webSocket.begin();
    webSocket.onEvent(webSocketEvent);
}

void loop() {
    processItem();
    webSocket.loop();
}