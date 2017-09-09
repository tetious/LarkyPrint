#pragma once

#include <utility>
#include <iostream>
#include <Preferences.h>
#include "nvs.h"
#include "Helpers.h"

#define nvs_error(e) (((e)>ESP_ERR_NVS_BASE)?nvs_errors[(e)&~(ESP_ERR_NVS_BASE)]:nvs_errors[0])

class Configuration {
    const vector<string> nvs_errors = {"OTHER", "NOT_INITIALIZED", "NOT_FOUND", "TYPE_MISMATCH", "READ_ONLY",
                                       "NOT_ENOUGH_SPACE",
                                       "INVALID_NAME", "INVALID_HANDLE", "REMOVE_FAILED", "KEY_TOO_LONG", "PAGE_FULL",
                                       "INVALID_STATE", "INVALID_LENGTH"};

    nvs_handle handle{};

    Configuration() {
        nvs_open("LarkyPrint", NVS_READWRITE, &handle);
        Preferences pre;

    }

    ~Configuration() {
        nvs_close(handle);
    }

    void logError(esp_err_t err, const string &message) {
        // todo: actually log properly
        nvs_errors[1];
        Serial.printf("Configuration: %s : %s", std::move(message), nvs_error(err));
    }

public:

    template<class T>
    T get(const char *key) {
        size_t size;
        T thing;
        auto err = nvs_get_blob(handle, key, nullptr, &size);
        if (err != ESP_OK) {
            logError(err, string("get len->").append(key));
        } else {
            if(size != sizeof(T)) {
                logError(0, string_format("get size mismatch for key %s: %u:%u", key, size, sizeof(T)));
            }
            err = nvs_get_blob(handle, key, &thing, &size);
            if (err != ESP_OK) {
                logError(err, string("get ->").append(key));
            }
            return thing;
        }

        return nullptr;
    }

    template<class T>
    void put(const char *key, T value) {
        auto err = nvs_set_blob(handle, key, &value, sizeof(T));
        if (err != ESP_OK) {
            logError(err, string("put::nvs_set_blob->").append(key));
        } else {
            err = nvs_commit(handle);
            if (err != ESP_OK) {
                logError(err, string("put::nvs_commit->").append(key));
            }
        }
    }

    Configuration(Configuration const &) = delete;

    Configuration &operator=(Configuration const &) = delete;

    static Configuration &Instance() {
        static Configuration i;
        return i;
    }
};