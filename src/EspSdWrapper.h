#pragma once

#include <cstdint>
#include <bits/stdc++.h>
#include <driver/sdmmc_types.h>
#include <ff.h>

struct FileInfo {
    std::string name;
    uint32_t size;
    std::string created;
};

class EspSdWrapper {
    bool mounted = false;
    FATFS *fs = nullptr;
    sdmmc_card_t *sdCard = nullptr;
    uint8_t pDrv = 0;
public:
    bool mount(bool = false);

    bool init(uint8_t miso, uint8_t mosi, uint8_t clk, uint8_t cs);

    void umount();

    std::vector<FileInfo> files(const char *dir);
};



