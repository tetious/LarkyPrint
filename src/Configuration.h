#pragma once

#include <Preferences.h>

#include <utility>
#include <iostream>
#include "nvs.h"

#define nvs_error(e) (((e)>ESP_ERR_NVS_BASE)?nvs_errors[(e)&~(ESP_ERR_NVS_BASE)]:nvs_errors[0])
#define TYPE_STRING(T) template<> const char* TypeName<T>::name = #T

template <typename T>
struct TypeName { static const char *name; };

struct EncoderConfiguration {
    boolean isReversed{};
    uint8_t stepsPerMenuMove = 1;
    uint8_t stepsPerValueAdjustment = 1;
};
TYPE_STRING(EncoderConfiguration);

class Configuration {
    const vector<string> nvs_errors = {"OTHER", "NOT_INITIALIZED", "NOT_FOUND", "TYPE_MISMATCH", "READ_ONLY", "NOT_ENOUGH_SPACE",
                                "INVALID_NAME", "INVALID_HANDLE", "REMOVE_FAILED", "KEY_TOO_LONG", "PAGE_FULL",
                                "INVALID_STATE", "INVALID_LENGTH"};

    nvs_handle handle{};

    Configuration() {
        nvs_open("LarkyPrint", NVS_READWRITE, &handle);
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
    T getStruct() {
        T _struct;
        size_t size;
        auto err = nvs_get_blob(handle, TypeName<EncoderConfiguration>::name, &_struct, &size);
        if (err != ESP_OK) {
            logError(err, string("getStruct->").append(TypeName<EncoderConfiguration>::name));
        }

        return _struct;
    }

    template<class T>
    void putStruct(T *value) {
        auto err = nvs_set_blob(handle, TypeName<EncoderConfiguration>::name, value, sizeof(T));
        if (err != ESP_OK) {
            logError(err, string("putStruct::nvs_set_blob->").append(TypeName<EncoderConfiguration>::name));
        } else {
            err = nvs_commit(handle);
            if (err != ESP_OK) {
                logError(err, string("putStruct::nvs_commit->").append(TypeName<EncoderConfiguration>::name));
            }
        }
    }

    static Configuration &Instance() {
        static Configuration i;
        return i;
    }
};