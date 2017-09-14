#include "Arduino.h"
#include <WiFi.h>
#include <ArduinoJson.h>
#include "secure.h"
#include "pins.h"
#include "ScreenBuffer.h"
#include "MenuManager.h"
#include <Update.h>
#include "WebSocketManager.h"
#include "EspSdWrapper.h"

using namespace WebSocket;

long lastUpdate = 0;

ScreenBuffer buffer = ScreenBuffer();
MenuManager menuManager = MenuManager(buffer);
EspSdWrapper sd;

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

void startScreenWatcher() {
    attachInterrupt(lcd_clk, read, FALLING);
}

void stopScreenWatcher() {
    detachInterrupt(lcd_clk);
}

IRAM_ATTR void sd_mode(bool espOwnsSd) {
    //stopScreenWatcher();
    if (espOwnsSd) {
        // trigger card removed signal and switch mux to ESP
        digitalWrite(sd_detect_out, HIGH);
        digitalWrite(sd_mux_s, LOW);
        delay(100);
        //TODO: we probably want to break this up into SPI init and mount
        if (!sd.mount(spi_miso, spi_mosi, spi_clk, sd_spi_ss)) {
            Serial.println("Card Mount Failed");
            // FIXME: if the SD card fails to mount, subsequent attempts crash the board
        } else {
//            uint8_t cardType = SD.cardType();
//
//            if (cardType == CARD_NONE) {
//                Serial.println("No SD card attached");
//            } else {
//                uint64_t cardSize = SD.cardSize() / (1024 * 1024);
//                Serial.printf("SD Card Size: %lluMB\n", cardSize);
//            }
        }
        Serial.println("The ESP now owns the SD card.");
    } else {
        //SD.end();
        // switch mux to printer and trigger card insert
        digitalWrite(sd_mux_s, HIGH);
        digitalWrite(sd_detect_out, LOW);
        Serial.println("The printer now owns the SD card.");
    }
    //startScreenWatcher();
}

void setupFirmwareUpdate() {
    auto &wsm = WebSocketManager::Instance();
    wsm.onOp("firmwareUpdateStart", [&](OperationMessage m) {
        Serial.println("Starting firmware update.");
        stopScreenWatcher();
        if (!Update.begin(m.root["fileSize"].as<size_t>())) {
            Serial.println("Update.begin error");
            Update.printError(Serial);
        } else {
            Update.setMD5(m.root["md5"]);
            wsm.attachBinaryHandler([&](BinaryMessage m) {
                auto written = Update.write(m.payload, m.payloadLength);
                if (written > 0) {
                    Serial.printf("firmwareUpdate: Added %u bytes. Update reports %u bytes.\r\n", m.payloadLength,
                                  written);
                    wsm.sendText(m.connectionNumber, R"({"op": "firmwareUpdateChunkAck"})");
                } else {
                    Serial.println("Written was zero.");
                    Update.printError(Serial);
                }
            });
        }
    });

    wsm.onOp("firmwareUpdateComplete", [&](OperationMessage m) {
        wsm.detachBinaryHandler();
        // todo: better messaging
        if (Update.end()) {
            Serial.println("OTA done!");
            if (Update.isFinished()) {
                Serial.println("Update successfully completed. Rebooting.");
                ESP.restart();
            } else {
                Serial.println("Update not finished? Something went wrong!");
                Update.printError(Serial);
                startScreenWatcher();
            }
        } else {
            Serial.println("Error Occurred. Error #: " + String(Update.getError()));
            Update.printError(Serial);
            startScreenWatcher();
        }
    });
}

String buildSdFileList() {
    DynamicJsonBuffer buf;
    auto &root = buf.createObject();
    root["op"] = "fileListing";
    auto &fileList = root.createNestedArray("files");

    for (auto file: sd.files("/sd")) {
        auto &fileObj = fileList.createNestedObject();
        fileObj["name"] = buf.strdup(file.name);
        fileObj["size"] = file.size;
    }

    String out;
    root.printTo(out);
    root.printTo(Serial);
    return out;
}

void setupUploadHandler() {
    static FILE *file = nullptr;

    // TODO:: make sure this doesn't get stuck (setup some kind of timeout)
    auto &wsm = WebSocketManager::Instance();
    wsm.onOp("fileUploadStart", [&](OperationMessage m) {
        sd_mode(true);
        Serial.printf("Starting file upload for file %s.\r\n", m.root["name"].as<const char *>());
        file = fopen(m.root["name"].as<const char *>(), "wb");
        if (file == nullptr) {
            Serial.println("Failed to open file for writing");
            //webSocket.sendTXT(num, "ERR"); // FIXME
            sd_mode(false);
        } else {
            wsm.attachBinaryHandler([&](BinaryMessage m) {
                fwrite(m.payload, m.payloadLength, 1, file); // this is so flipping baffling
                Serial.printf("fileUpload: Added %u bytes.\r\n", m.payloadLength);
                wsm.sendText(m.connectionNumber, R"({"op": "fileUploadChunkAck"})");
            });
        }
    });

    wsm.onOp("fileUploadComplete", [&](OperationMessage m) {
        wsm.detachBinaryHandler();
        Serial.println("Closing file.");
        fclose(file);
        file = nullptr;
        sd_mode(false);
    });
}

void initWifi() {
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println();
    Serial.println("WiFi connected, OTA update ready.");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
}

void initWebsockets() {
    setupUploadHandler();
    setupFirmwareUpdate();
    auto &wsm = WebSocketManager::Instance();
    wsm.onOp("menuClick", [](OperationMessage m) {
        Serial.println("CLICK!");
        menuManager.click();
    });
    wsm.onOp("menuUp", [](OperationMessage m) {
        Serial.println("UP!");
        menuManager.up();
    });
    wsm.onOp("menuDown", [](OperationMessage m) {
        Serial.println("DOWN!");
        menuManager.down();
    });
    wsm.onOp("fileListing", [&](OperationMessage m) {
        sd_mode(true);
        wsm.sendText(m.connectionNumber, buildSdFileList().c_str());
        sd_mode(false);
    });
}

void initScreen() {
    buffer.subUpdate([](ScreenBuffer *buf) {
        const size_t bufferSize = JSON_ARRAY_SIZE(80) + JSON_OBJECT_SIZE(2) + 380;
        DynamicJsonBuffer jsonBuffer(bufferSize);
        auto &obj = jsonBuffer.createObject();
        obj["op"] = "screenUpdate";
        auto &screenArray = obj.createNestedArray("s");
        for (auto i: buf->read()) {
            screenArray.add(int(i));
        }

        String out;
        obj.printTo(out);
        WebSocketManager::Instance().broadcast(out.c_str());
    });
}

void setup() {
    Serial.begin(115200);
    Serial.setDebugOutput(true);

    for (auto lcd_pin : lcd_pins) {
        pinMode(lcd_pin, INPUT_PULLDOWN);
    }
    pinMode(lcd_clk, INPUT_PULLDOWN);
    pinMode(lcd_rs, INPUT_PULLDOWN);

    pinMode(sd_detect_out, OUTPUT);
    pinMode(sd_mux_s, OUTPUT);
    //sdSpi.begin(spi_clk, spi_miso, spi_mosi);
    sd_mode(false);

    //digitalWrite(spi_clk, HIGH);
    initWifi();
    configTzTime("EST", "pool.ntp.org");
    initWebsockets();
    initScreen();
    startScreenWatcher();

    Serial.printf("Free heap: %u\r\n", ESP.getFreeHeap());
}

void loop() {
    const auto _millis = millis();
    static unsigned long lastMillis = 0;

    WebSocketManager::Instance().loop();
    lastMillis = _millis;
}