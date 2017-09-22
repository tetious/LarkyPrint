
#include "Arduino.h"
#include <WiFi.h>
#include <ArduinoJson.h>
#include "secure.h"
#include "pins.h"
#include "ScreenBuffer.h"
#include "menu/MenuManager.h"
#include <Update.h>
#include "ESPAsyncWebServer.h"
#include "EventSourceManager.h"
#include "EspSdWrapper.h"
#include "ScreenWatcher.h"
#include "esp_ota_ops.h"
#include "html.h"
#include "bits/stdc++.h"
#include "Helpers.h"
#include "Configuration.h"
#include "WifiHelper.h"

using namespace placeholders;

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

            if (size == "") {
                log_w("size not found.");
                return;
            }

            if (md5 == "") {
                log_w("md5 not found.");
            }

            Serial.println("Starting firmware update.");
            log_d("before firmware update, bp: %x, cp: %x", esp_ota_get_boot_partition()->address,
                  esp_ota_get_running_partition()->address);
            ScreenWatcher::stop();
            if (!Update.begin(static_cast<size_t>(atoi(size.c_str())))) {
                Serial.println("Update.begin error");
                Update.printError(Serial);
            } else if (md5.length() > 0) {
                updateRunning = Update.setMD5(md5.c_str());
            }
        }

        if (!updateRunning) { return; }

        auto written = Update.write(data, len);
        if (written == 0) {
            Serial.println("Written was zero.");
            Update.printError(Serial);
            updateRunning = false;
        } else {
            //Serial.printf("%u -> %u\r\n", written, Update.remaining());
        }
        if (final) {
            if (Update.end()) {
                Serial.println("OTA done!");
                if (Update.isFinished()) {
                    log_d("after firmware update, bp: %x, cp: %x", esp_ota_get_boot_partition()->address,
                          esp_ota_get_running_partition()->address);
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

JsonObject &GetJsonRoot(DynamicJsonBuffer &jsonBuffer, uint8_t *data) {
    auto &root = jsonBuffer.parseObject(data);
    if (!root.success()) {
        log_e("error parsing JSON [%s].", data);
    }

    return root;
}

void connectWifi() {
    auto &config = Configuration::Instance();
    auto t_ssid = config.getS("wifi.ssid");
    auto t_password = config.getS("wifi.password");
    if (t_ssid.empty() || t_password.empty()) {
        log_i("Could not load wifi creds from storage. Falling back to hardcoded values.");
        WiFi.begin(ssid, password);
    } else {
        log_i("wifi: %s, %s", t_ssid.c_str(), t_password.c_str());
        WiFi.begin(t_ssid.c_str(), t_password.c_str());
    }
}

void initWebServer() {
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
    webServer.on("/menu/sdPrint", HTTP_POST, [](AsyncWebServerRequest *request) {
        request->send(200);
    }, nullptr, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        if (total > len) {
            log_e("index > 0!");
            return;
        }
        DynamicJsonBuffer jsonBuffer;
        auto &root = GetJsonRoot(jsonBuffer, data);
        menuManager.printFile(root["f"], [&](bool s) {
            wsm.broadcast(s ? "true" : "false", "sdPrint");
        });
    });

    webServer.on("/sd", HTTP_GET, [](AsyncWebServerRequest *request) {
        auto *response = request->beginResponseStream("text/json");
        sd_mode(true);
        buildSdFileList(response);
        sd_mode(false);
        request->send(response);
    });

    webServer.on("/wifi", HTTP_DELETE, [](AsyncWebServerRequest *request) {
        WiFi.disconnect();
        auto &config = Configuration::Instance();
        config.remove("wifi.ssid");
        config.remove("wifi.password");
        log_d("Cleared WiFi creds and disconnected.");
        request->send(200);
    });
    webServer.on("/wifi", HTTP_POST, [](AsyncWebServerRequest *request) {
    }, nullptr, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        if (total > len) {
            log_e("index > 0!");
            request->send(500);
            return;
        }
        DynamicJsonBuffer jsonBuffer;
        auto &root = GetJsonRoot(jsonBuffer, data);
        if (!root.success()) {
            log_e("/sd/print error parsing JSON!");
        }
        auto &config = Configuration::Instance();
        string ssid = root["ssid"];
        string password = root["password"];
        if (ssid.empty()) {
            request->send(401);
            log_d("SSID is empty");
            return;
        }
        config.putS("wifi.ssid", ssid);
        config.putS("wifi.password", password);
        wifi_change_creds(ssid.c_str(), password.c_str());
        request->send(200);
    });

    webServer.on("/wifi/status", HTTP_GET, [](AsyncWebServerRequest *request) {
        auto json = string_format(R"({"status": %u, "ssid": "%s", "ip": "%s"})",
                                  WiFi.status(), WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
        request->send(200, "text/json", json.c_str());
    });

    webServer.on("/wifi", HTTP_GET, [](AsyncWebServerRequest *request) {
        string json = "[";
        int n = WiFi.scanComplete();
        if (n == WIFI_SCAN_FAILED) {
            WiFi.scanNetworks(true);
        } else if (n) {
            for (uint8_t i = 0; i < n; ++i) {
                if (i) { json += ","; }
                json += string_format(R"({"ssid": "%s", "rssi": %i,"secure": %s})",
                                      WiFi.SSID(i).c_str(),
                                      WiFi.RSSI(i),
                                      WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "false" : "true");
            }
        }
        json += "]";
        request->send(200, "text/json", json.c_str());
        WiFi.scanNetworks(true);
    });

    webServer.on("/fw", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/html", update_html);
    });
    webServer.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/html", wifi_html);
    });

    webServer.on("/app.css", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/css", app_css);
    });
    webServer.on("/mini-default.min.css", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/css", mini_default_min_css);
    });
    webServer.on("/app.js", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "application/javascript", app_js);
    });

    webServer.onNotFound([](AsyncWebServerRequest *request) {
        if (request->method() == HTTP_OPTIONS) {
            request->send(200);
        } else {
            request->send(404);
        }
    });
}

void initScreenEvents() {
    ScreenWatcher::screenBuffer.subUpdate([](ScreenBuffer *buf, bool _1) {
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


void initWifi() {
    WiFi.onEvent([](WiFiEvent_t event) {
        Serial.printf("[WiFi-event] event: %d\r\n", event);

        switch (event) {
            case SYSTEM_EVENT_STA_GOT_IP:
                Serial.println("WiFi connected.");
                Serial.println("IP address: ");
                Serial.print(WiFi.localIP());
                Serial.println();
                break;
            case SYSTEM_EVENT_STA_DISCONNECTED:
                log_w("WiFi lost connection: %u", WiFi.status());
                break;
        }
    });
    WiFi.softAP("LarkyPrint");
    connectWifi();
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
    sd_mode(true);
    sd.init(spi_miso, spi_mosi, spi_clk, sd_spi_ss);
    sd_mode(false);

    initWifi();
    configTzTime("EST", "pool.ntp.org");

    initWebServer();

    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
    webServer.begin();

    initScreenEvents();
    ScreenWatcher::start();

    Serial.printf("Free heap: %u\r\n", ESP.getFreeHeap());
    log_d("bp: %x, cp: %x", esp_ota_get_boot_partition()->address,
          esp_ota_get_running_partition()->address);
}

void loop() {
    if (restartNow) {
        Serial.println("Rebooting...");
        delay(100);
        ESP.restart();
    }
    delay(100);
}