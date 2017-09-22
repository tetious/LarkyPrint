#pragma once

#include <utility>
#include <iostream>
#include <Preferences.h>
#include "nvs.h"
#include "Helpers.h"


class Configuration {
    Preferences p{};
public:

    Configuration(){
        p.begin("LarkyPrint");
        log_d("Started.");
    }

    void putS(string key, string value) {
        p.putString(key.c_str(), value.c_str());
    }

    string getS(string key) {
        return string{p.getString(key.c_str()).c_str()};
    }

    void remove(string key) {
        p.remove(key.c_str());
    }

    Configuration(Configuration const &) = delete;

    Configuration &operator=(Configuration const &) = delete;

    static Configuration &Instance() {
        static Configuration i;
        return i;
    }
};