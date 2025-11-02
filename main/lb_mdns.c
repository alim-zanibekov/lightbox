#include "lb_mdns.h"
#include <esp_err.h>
#include <esp_log.h>
#include <mdns.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "lb_settings.h"

static const char* TAG = "lb_mdns";

static void settings_listen_task(void* _)
{
    // ReSharper disable once CppDFAEndlessLoop
    for (;;)
    {
        xEventGroupWaitBits(x_settings_event_handle, SETTINGS_UPDATED_BIT,
                            pdTRUE, pdFALSE, portMAX_DELAY);
        const lb_settings settings = lb_settings_get();
        ESP_ERROR_CHECK(mdns_hostname_set(settings.mdns.hostname));
        ESP_LOGI(TAG, "mdns hostname set to: [%s]", settings.mdns.hostname);
        ESP_ERROR_CHECK(mdns_instance_name_set(settings.mdns.device_name));
        ESP_LOGI(TAG, "mdns instance name set to: [%s]", settings.mdns.device_name);
    }
}

void lb_initialise_mdns()
{
    ESP_ERROR_CHECK(mdns_init());
    const lb_settings settings = lb_settings_get();
    ESP_ERROR_CHECK(mdns_hostname_set(settings.mdns.hostname));
    ESP_LOGI(TAG, "mdns hostname set to: [%s]", settings.mdns.hostname);
    ESP_ERROR_CHECK(mdns_instance_name_set(settings.mdns.device_name));
    ESP_LOGI(TAG, "mdns instance name set to: [%s]", settings.mdns.device_name);
    mdns_txt_item_t serviceTxtData[1] = {{"board", "esp32"}};
    ESP_ERROR_CHECK(mdns_service_add(NULL, "_http", "_tcp", 80, serviceTxtData, 1));
    ESP_ERROR_CHECK(mdns_service_subtype_add_for_host(NULL, "_http", "_tcp", NULL, "_server"));

    xTaskCreate(settings_listen_task, "mdns_settings_listen_task", configMINIMAL_STACK_SIZE + 2048,
                NULL, 5, NULL);
}
