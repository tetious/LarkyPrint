#include "Arduino.h"
#include <WiFi.h>
#include <WebSocketsServer.h>
#include "SPI.h"
#include "FS.h"
#include "SD.h"
#include <ArduinoJson.h>
#include "secure.h"
#include "pins.h"
#include "ScreenBuffer.h"
#include "MenuManager.h"

long lastUpdate = 0;

WebSocketsServer webSocket = WebSocketsServer(80);
ScreenBuffer buffer = ScreenBuffer();
SPIClass sdSpi = SPIClass();
MenuManager menuManager = MenuManager(buffer);

IRAM_ATTR void read() {
    static bool firstHalf = true;
    static lcd_cap current = lcd_cap();

    if (digitalRead(lcd_rs) == 0) {
        current.cmd = true;
    }

    // full byte is broken into two, so stuff them into the correct place
    for (int pin = 0; pin < sizeof(lcd_pins); pin++) {
        current.data |= (digitalRead(lcd_pins[pin]) << pin + (firstHalf ? 4 : 0));
    }

    if (!firstHalf) {
        buffer.write(current);
        current = lcd_cap();
    }

    firstHalf = !firstHalf;
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
    static auto fileUploading = false;
    static File file;

    switch (type) {
        case WStype_DISCONNECTED:
            Serial.printf("[%u] Disconnected!\r\n", num);
            break;
        case WStype_CONNECTED: {
            IPAddress ip = webSocket.remoteIP(num);
            Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\r\n", num, ip[0], ip[1], ip[2], ip[3], payload);
        }
            break;
        case WStype_TEXT: {
            Serial.printf("[%u] get Text: %s\r\n", num, payload);

            DynamicJsonBuffer jsonBuffer;
            JsonObject &root = jsonBuffer.parseObject(payload);
            if (root["op"] == "fileUploadStart") {
                Serial.printf("Starting file upload for file %s.\r\n", root["name"].as<String>());
                file = SD.open(root["name"].as<String>(), FILE_WRITE);
                if (!file) {
                    Serial.println("Failed to open file for writing");
                    webSocket.sendTXT(num, "ERR"); // FIXME
                } else {
                    fileUploading = true;
                }
            } else if (root["op"] == "fileUploadComplete") {
                Serial.println("Closing file.");
                fileUploading = false;
                file.close();
            } else if (root["op"] == "menuClick") {
                Serial.println("CLICK!");
                menuManager.click();
            } else if (root["op"] == "menuUp") {
                Serial.println("UP!");
                menuManager.up();
            } else if (root["op"] == "menuDown") {
                Serial.println("DOWN!");
                menuManager.down();
            }
            break;
        }
        case WStype_BIN: {
            Serial.printf("[%u] get binary length: %u\r\n", num, length);

            if (fileUploading) {
                file.write(payload, length);
                Serial.printf("fileUpload: Added %u bytes.\r\n", length);
                webSocket.sendTXT(num, R"({"op": "fileUploadChunkAck"})");
            }
            break;
        }
    }
}

void sd_setup() {
    if (!SD.begin(sd_spi_ss, sdSpi)) {
        Serial.println("Card Mount Failed");
        return;
    }
    uint8_t cardType = SD.cardType();

    if (cardType == CARD_NONE) {
        Serial.println("No SD card attached");
        return;
    }

    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    Serial.printf("SD Card Size: %lluMB\n", cardSize);
}

void setup() {
    Serial.begin(115200);
    Serial.setDebugOutput(true);
    sdSpi.begin(spi_clk, spi_miso, spi_mosi, sd_spi_ss);
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println();
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());

    sd_setup();

    for (auto lcd_pin : lcd_pins) {
        pinMode(lcd_pin, INPUT_PULLDOWN);
    }

    pinMode(lcd_clk, INPUT_PULLDOWN);
    pinMode(lcd_rs, INPUT_PULLDOWN);
    attachInterrupt(lcd_clk, read, FALLING);

    webSocket.begin();
    webSocket.onEvent(webSocketEvent);

    Serial.printf("Free heap: %u\r\n",ESP.getFreeHeap());
}

void loop() {
    const auto _millis = millis();

    if (lastUpdate == 0 || _millis - lastUpdate > 1000) {
        String status;
        for (auto chr : buffer.read()) {
            if (chr > 31) status.concat(chr);
        }
        //todo webSocket.broadcastTXT(status);
        Serial.println(status);
        lastUpdate = _millis;
    }
    webSocket.loop();
    menuManager.loop(_millis);
}