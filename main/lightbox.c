#include <mdns.h>
#include <nvs_flash.h>
#include <stdio.h>
#include <sys/unistd.h>

#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_littlefs.h"
#include "lb_gallery.h"

#include "lb_wifi.h"

#include "lb_settings.h"
#include "lb_server.h"
#include "lb_mdns.h"
#include "lb_led_matrix.h"
#include "lb_ota.h"

static const char* TAG = "lb_main";

void app_main(void)
{
    lb_ota_init();

    ESP_LOGI(TAG, "Initializing NVS");
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // OTA app partition table has a smaller NVS partition size than the non-OTA
        // partition table. This size mismatch may cause NVS initialization to fail.
        // If this happens, we erase NVS partition and initialize NVS again.
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    ESP_LOGI(TAG, "Initializing LittleFS");
    const esp_vfs_littlefs_conf_t conf = {
        .base_path = "/lfs",
        .partition_label = "littlefs",
        .format_if_mount_failed = true,
        .dont_mount = false,
    };
    ESP_ERROR_CHECK(esp_vfs_littlefs_register(&conf));

    size_t total = 0, used = 0;
    const int ret = esp_littlefs_info(conf.partition_label, &total, &used);
    if (esp_littlefs_info(conf.partition_label, &total, &used) != ESP_OK)
        ESP_LOGE(TAG, "Failed to get LittleFS partition information (%s)", esp_err_to_name(ret));
    else
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);


    lb_settings_init();
    lb_gallery_init();
    lb_led_matrix_init();
    static lb_server_handle server = NULL;
    lb_init_wifi();
    lb_initialise_mdns();
    server = lb_start_webserver();
    while (server)
    {
        sleep(5);
    }
    esp_vfs_littlefs_unregister(conf.partition_label);
    ESP_LOGI(TAG, "LittleFS unmounted");
}
