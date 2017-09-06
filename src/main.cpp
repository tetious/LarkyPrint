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

auto sdSpi = SPIClass();
IRAM_ATTR void sd_mode(bool espOwnsSd) {

    if(espOwnsSd) {
        // trigger card removed signal and switch mux to ESP
        digitalWrite(sd_detect_out, HIGH);
        digitalWrite(sd_mux_s, LOW);
        delay(100);
        if (!SD.begin(sd_spi_ss, sdSpi)) {
            Serial.println("Card Mount Failed");
            // FIXME: if the SD card fails to mount, subsequent attempts crash the board
        } else {
            uint8_t cardType = SD.cardType();

            if (cardType == CARD_NONE) {
                Serial.println("No SD card attached");
            } else {
                uint64_t cardSize = SD.cardSize() / (1024 * 1024);
                Serial.printf("SD Card Size: %lluMB\n", cardSize);
            }
        }

        Serial.println("The ESP now owns the SD card.");
    } else {
        SD.end();
        // switch mux to printer and trigger card insert
        digitalWrite(sd_mux_s, HIGH);
        digitalWrite(sd_detect_out, LOW);
        Serial.println("The printer now owns the SD card.");
    }
}

// TODO: rewrite all of this.
IRAM_ATTR void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
    static auto fileUploading = false;
    static File file;
    static bool sdMode = true;

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
            Serial.printf("[%u] Got Text: %s\r\n", num, payload);

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
            } else if (root["op"] == "swapSD") {
                Serial.println("SD SWAP!");
                sdMode =!sdMode;
                sd_mode(sdMode);
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

void setup() {
    Serial.begin(115200);
    Serial.setDebugOutput(true);
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println();
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());

    for (auto lcd_pin : lcd_pins) {
        pinMode(lcd_pin, INPUT_PULLDOWN);
    }
    pinMode(lcd_clk, INPUT_PULLDOWN);
    pinMode(lcd_rs, INPUT_PULLDOWN);

    webSocket.begin();
    webSocket.onEvent(webSocketEvent);

    pinMode(sd_detect_out, OUTPUT);
    pinMode(sd_mux_s, OUTPUT);
    sdSpi.begin(spi_clk, spi_miso, spi_mosi);
    sd_mode(true);

    // do this last as maybe that will help with SD init? This is unproven.
    attachInterrupt(lcd_clk, read, FALLING);

    Serial.printf("Free heap: %u\r\n", ESP.getFreeHeap());
}

void loop() {
    const auto _millis = millis();
    if (lastUpdate == 0 || _millis - lastUpdate > 1000) {
        String status;
        for (auto chr : buffer.read()) {
            if (chr > 31) status.concat(chr);
        }
        // todo: this should probably be event driven and work more intelligently
        StaticJsonBuffer<200> json;
        auto& root = json.createObject();
        root["op"] = "printerStatus";
        root["status"] = status;
        String out;
        root.printTo(out);
        webSocket.broadcastTXT(out);
        Serial.println(status);

        lastUpdate = _millis;
    }
    webSocket.loop();
    menuManager.loop(_millis);
}