#pragma once

#include <esp_wifi.h>

bool wifi_change_creds(const char* ssid, const char *passphrase)
{
    if(!ssid || *ssid == 0x00 || strlen(ssid) > 31) {
        log_e("ssid is too long or missing");
        return false;
    }

    if(passphrase && strlen(passphrase) > 63) {
        log_e("passphrase is too long");
        return false;
    }

    wifi_config_t current_conf{};
    esp_wifi_get_config(WIFI_IF_STA, &current_conf);

    strcpy(reinterpret_cast<char*>(current_conf.sta.ssid), ssid);

    if(passphrase) {
        strcpy(reinterpret_cast<char*>(current_conf.sta.password), passphrase);
    } else {
        *current_conf.sta.password = 0;
    }

    esp_wifi_set_config(WIFI_IF_STA, &current_conf);

    esp_wifi_disconnect();
    esp_wifi_connect();

    return true;
}