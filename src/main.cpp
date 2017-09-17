#include "LarkyPrint.h"
#include "Arduino.h"
#include <WiFi.h>
#include <ArduinoJson.h>
#include "secure.h"
#include "pins.h"
#include "ScreenBuffer.h"
#include "MenuManager.h"
#include <Update.h>
#include "ESPAsyncWebServer.h"
#include "EventSourceManager.h"
#include "EspSdWrapper.h"
#include "ScreenWatcher.h"


MenuManager menuManager = MenuManager(ScreenWatcher::screenBuffer);
EspSdWrapper sd;
AsyncWebServer webServer{80};
EventSourceManager wsm{webServer};

auto restartNow = false;

void sd_mode(bool espOwnsSd) {
    if (espOwnsSd) {
        // trigger card removed signal and switch mux to ESP
        digitalWrite(sd_detect_out, HIGH);
        digitalWrite(sd_mux_s, LOW);
        //TODO: we probably want to break this up into SPI init and mount
        if (!sd.mount()) {
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
        sd.umount();
        // switch mux to printer and trigger card insert
        digitalWrite(sd_mux_s, HIGH);
        digitalWrite(sd_detect_out, LOW);
        Serial.println("The printer now owns the SD card.");
    }
}

void setupFirmwareUpdate() {
    webServer.on("/fw", HTTP_POST, [](AsyncWebServerRequest *request) {
        restartNow = !Update.hasError();
        AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", restartNow ? "OK" : "FAIL");
        response->addHeader("Connection", "close");
        request->send(response);
    }, [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
        static auto updateRunning = false;
        if (!index) {
            auto md5 = request->arg("md5");
            auto size = request->arg("size");

            if (md5 == "" || size == "") {
                Serial.println("md5 or size not found.");
                return;
            }

            Serial.println("Starting firmware update.");
            ScreenWatcher::stop();
            if (!Update.begin(static_cast<size_t>(atoi(size.c_str())))) {
                Serial.println("Update.begin error");
                Update.printError(Serial);
            } else {
                updateRunning = Update.setMD5(md5.c_str());
            }
        }

        if (!updateRunning) { return; }

        auto written = Update.write(data, len);
        if (written > 0) {
            Serial.printf("firmwareUpdate: %u\r\n", Update.remaining());
        } else {
            Serial.println("Written was zero.");
            Update.printError(Serial);
            updateRunning = false;
        }

        if (final) {
            if (Update.end()) {
                Serial.println("OTA done!");
                if (Update.isFinished()) {
                    Serial.println("Update successfully completed. Rebooting.");
                    restartNow = true;
                } else {
                    Serial.println("Update not finished? Something went wrong!");
                    Update.printError(Serial);
                    ScreenWatcher::start();
                }
            } else {
                Serial.println("Error Occurred. Error #: " + String(Update.getError()));
                Update.printError(Serial);
                ScreenWatcher::start();
            }
        }
    });
}

void buildSdFileList(AsyncResponseStream *response) {
    DynamicJsonBuffer buf;
    auto &root = buf.createObject();
    root["op"] = "fileListing";
    auto &fileList = root.createNestedArray("files");

    for (auto file: sd.files("/sd")) {
        auto &fileObj = fileList.createNestedObject();
        fileObj["name"] = file.name;
        fileObj["size"] = file.size;
        fileObj["created"] = file.created;
    }

    root.printTo(*response);
}

void setupUploadHandler() {
    static FILE *file = nullptr;

    webServer.on("/sd", HTTP_POST, [](AsyncWebServerRequest *request) {
        AsyncWebServerResponse *response = request->beginResponse(200, "text/plain");
        response->addHeader("Connection", "close");
        request->send(response);
    }, [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
        if (!index) {
            sd_mode(true);
            Serial.printf("Upload Start: %s\r\n", filename.c_str());
            file = fopen(("/sd/" + filename).c_str(), "wb");
            if (file == nullptr) {
                Serial.println("Failed to open file for writing");
                sd_mode(false);
                return;
                // TODO how to abort?
            }
        }
        if (file == nullptr) { return; } // FIXME

        fwrite(data, len, 1, file);

        if (final) {
            Serial.println("Closing file.");
            fclose(file);
            file = nullptr;
            sd_mode(false);
        }
    });
}

void initWifi() {
    WiFi.onEvent([](WiFiEvent_t event) {
        Serial.printf("[WiFi-event] event: %d\r\n", event);

        switch (event) {
            case SYSTEM_EVENT_STA_GOT_IP:
                Serial.println("WiFi connected.");
                Serial.println("IP address: ");
                Serial.println(WiFi.localIP());
                break;
            case SYSTEM_EVENT_STA_DISCONNECTED:
                Serial.println("WiFi lost connection");
                break;
        }
    });

    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
}

void initWebsockets() {
    setupUploadHandler();
    setupFirmwareUpdate();
    webServer.on("/menu/click", HTTP_GET, [](AsyncWebServerRequest *request) {
        Serial.println("CLICK!");
        menuManager.click();
        request->send(200);
    });
    webServer.on("/menu/up", HTTP_GET, [](AsyncWebServerRequest *request) {
        Serial.println("UP!");
        menuManager.up();
        request->send(200);
    });
    webServer.on("/menu/down", HTTP_GET, [](AsyncWebServerRequest *request) {
        Serial.println("DOWN!");
        menuManager.down();
        request->send(200);
    });
    webServer.on("/sd", HTTP_GET, [](AsyncWebServerRequest *request) {
        auto *response = request->beginResponseStream("text/json");
        sd_mode(true);
        buildSdFileList(response);
        sd_mode(false);
        request->send(response);
    });
}

void initScreenEvents() {
    ScreenWatcher::screenBuffer.subUpdate([](ScreenBuffer *buf) {
        const size_t bufferSize = JSON_ARRAY_SIZE(80) + JSON_OBJECT_SIZE(2) + 380;
        DynamicJsonBuffer jsonBuffer(bufferSize);
        auto &root = jsonBuffer.createObject();
        root["op"] = "screenUpdate";
        auto &screenArray = root.createNestedArray("s");
        for (auto i: buf->read()) {
            screenArray.add(int(i));
        }

        string out;
        root.printTo(out);
        wsm.broadcast(out.c_str(), "screenUpdate");
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
    sd_mode(true);
    sd.init(spi_miso, spi_mosi, spi_clk, sd_spi_ss);
    sd_mode(false);

    //digitalWrite(spi_clk, HIGH);
    initWifi();
    configTzTime("EST", "pool.ntp.org");
    initWebsockets();


    webServer.onNotFound([](AsyncWebServerRequest *request) {
        if (request->method() == HTTP_OPTIONS) {
            request->send(200);
        } else {
            request->send(404);
        }
    });

    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
    webServer.begin();

    initScreenEvents();
    ScreenWatcher::start();

    Serial.printf("Free heap: %u\r\n", ESP.getFreeHeap());
}

void loop() {
    if (restartNow) {
        Serial.println("Rebooting...");
        delay(100);
        ESP.restart();
    }
    delay(100);
}