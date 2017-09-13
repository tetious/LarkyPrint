#include <esp32-hal-log.h>
#include "EspSdWrapper.h"
#include "driver/sdspi_host.h"
#include "esp_vfs_fat.h"
#include <dirent.h>

using namespace std;

bool EspSdWrapper::mount(uint8_t miso, uint8_t mosi, uint8_t clk, uint8_t cs) {
    if(mounted) { return true; }

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.max_freq_khz = 10000;
    sdspi_slot_config_t slot_config = SDSPI_SLOT_CONFIG_DEFAULT();
    slot_config.gpio_miso = static_cast<gpio_num_t>(miso);
    slot_config.gpio_mosi = static_cast<gpio_num_t>(mosi);
    slot_config.gpio_sck = static_cast<gpio_num_t>(clk);
    slot_config.gpio_cs = static_cast<gpio_num_t>(cs);

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
            .format_if_mount_failed = false,
            .max_files = 5
    };

    esp_err_t ret = esp_vfs_fat_sdmmc_mount("/sd", &host, &slot_config, &mount_config, nullptr);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            log_e("Failed to mount filesystem.");
        } else {
            log_e("Failed to initialize the card (%d). ", ret);
        }
        return false;
    }
    mounted = true;
    return true;
}

std::vector<FileInfo> EspSdWrapper::files(const char *dir) {
    auto *dp = opendir(dir);
    struct dirent *ep;
    vector<FileInfo> files;

    if (dp != nullptr) {
        while ((ep = readdir(dp))) {
            FILINFO sstat{};
            f_stat(ep->d_name, &sstat);
            files.push_back({strdup(ep->d_name), static_cast<uint32_t>(sstat.fsize)});
        }
        closedir(dp);
    } else {
        log_e("Couldn't open the directory %s", dir);
    }

    return files;
}

void EspSdWrapper::umount() {
    esp_vfs_fat_sdmmc_unmount();
}


