#pragma once

#include <cstdint>
#include <bits/stdc++.h>
#include <driver/sdmmc_types.h>

struct FileInfo {
    const char * name;
    uint32_t size;
};

class EspSdWrapper {
    bool mounted = false;
    sdmmc_card_t *card{};
public:
    bool mount(uint8_t miso, uint8_t mosi, uint8_t clk, uint8_t cs);

    void umount();

    std::vector<FileInfo> files(const char *dir);
};



