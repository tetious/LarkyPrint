#include <esp32-hal-log.h>
#include "EspSdWrapper.h"
#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "diskio.h"
#include "FatHelpers.h"

using namespace std;


bool EspSdWrapper::init(uint8_t miso, uint8_t mosi, uint8_t clk, uint8_t cs) {
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
    char drv[3] = {(char) ('0' + pDrv), ':', 0};;
    esp_err_t err = ESP_OK;

    // connect SDMMC driver to FATFS
    BYTE pdrv = 0xFF;
    if (ff_diskio_get_drive(&pdrv) != ESP_OK || pdrv == 0xFF) {
        log_e("the maximum count of volumes is already mounted");
        return false;
    }

    sdCard = static_cast<sdmmc_card_t *>(malloc(sizeof(sdmmc_card_t)));
    if (sdCard == nullptr) {
        log_e("Out of memory.");
        goto fail;
    }

    err = (*host.init)();
    if (err != ESP_OK) {
        log_e("host init returned rc=0x%x", err);
        goto fail;
    }

    err = sdspi_host_init_slot(host.slot,
                               (const sdspi_slot_config_t *) &slot_config);
    if (err != ESP_OK) {
        log_e("slot_config returned rc=0x%x", err);
        goto fail;
    }

    err = sdmmc_card_init(&host, sdCard);
    if (err != ESP_OK) {
        log_e("sdmmc_card_init failed 0x(%x)", err);
        goto fail;
    }

    ff_diskio_register_sdmmc(pdrv, sdCard);
    log_d("using pdrv=%i", pdrv);
    drv[0] = '0' + pDrv;

    err = esp_vfs_fat_register("/sd", drv, mount_config.max_files, &fs);
    if (err == ESP_ERR_INVALID_STATE) {
        // it's okay, already registered with VFS
    } else if (err != ESP_OK) {
        log_e("esp_vfs_fat_register failed 0x(%x)", err);
        goto fail;
    }

    return true;

    fail:
    sdmmc_host_deinit();
    if (fs) {
        f_mount(nullptr, drv, 0);
    }
    fs = nullptr;
    pDrv = 0;
    esp_vfs_fat_unregister_path("/sd");
    ff_diskio_unregister(pdrv);
    free(sdCard);
    sdCard = nullptr;
    return false;
}

bool EspSdWrapper::mount(bool formatIfNeeded) {
    if (mounted) { return true; }

    const size_t workbuf_size = 4096;
    void *workbuf = nullptr;
    char drv[3] = {(char) ('0' + pDrv), ':', 0};

    // Try to mount partition
    FRESULT res = f_mount(fs, drv, 1);
    esp_err_t err = ESP_OK;

    if (res != FR_OK) {
        err = ESP_FAIL;
        log_w("failed to mount card (%d)", res);
        if (!(res == FR_NO_FILESYSTEM && !formatIfNeeded)) {
            goto fail;
        }
        log_w("partitioning card");
        DWORD plist[] = {100, 0, 0, 0};
        workbuf = malloc(workbuf_size);
        res = f_fdisk(pDrv, plist, workbuf);
        if (res != FR_OK) {
            err = ESP_FAIL;
            log_d("f_fdisk failed (%d)", res);
            goto fail;
        }
        log_w("formatting card");
        res = f_mkfs(drv, FM_ANY, sdCard->csd.sector_size, workbuf, workbuf_size);
        if (res != FR_OK) {
            err = ESP_FAIL;
            log_d("f_mkfs failed (%d)", res);
            goto fail;
        }
        free(workbuf);
        workbuf = nullptr;
        log_w("mounting again");
        res = f_mount(fs, drv, 0);
        if (res != FR_OK) {
            err = ESP_FAIL;
            log_d("f_mount failed after formatting (%d)", res);
            goto fail;
        }
    }

    mounted = true;
    return true;

    fail:
    mounted = false;
    free(workbuf);
    workbuf = nullptr;
    return false;
}

std::vector<FileInfo> EspSdWrapper::files(const char *dir) {
    auto *dp = opendir(dir);
    struct dirent *ep;
    vector<FileInfo> files;

    if (dp != nullptr) {
        while ((ep = readdir(dp))) {
            FILINFO sstat{};
            f_stat(ep->d_name, &sstat);
            char isoDate[20];
            auto time = date_dos2unix(sstat.ftime, sstat.fdate);
            strftime(isoDate, 20, "%FT%TZ", localtime(&time));
            files.push_back({string(ep->d_name), static_cast<uint32_t>(sstat.fsize), string(isoDate)});
        }
        closedir(dp);
    } else {
        log_e("Couldn't open the directory %s", dir);
    }

    return files;
}

void EspSdWrapper::umount() {
    char drv[3] = {(char) ('0' + pDrv), ':', 0};
    f_mount(nullptr, drv, 0);
    mounted = false;
}


