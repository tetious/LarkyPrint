#pragma once

#include <cstdint>
#include <bits/stdc++.h>

class EspSdWrapper {
public:
    bool mount(uint8_t miso, uint8_t mosi, uint8_t clk, uint8_t cs);

    void umount();

    std::vector<std::string> files(const char *dir);
};



