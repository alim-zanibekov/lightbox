#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_mac.h"
#include "esp_timer.h"
#include "lb_settings.h"

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1
#define WIFI_SWITCH_TO_AP BIT3
#define WIFI_NEW_SETTINGS BIT4

#define DHCPS_OFFER_DNS 0x02

static const int STA_RETRY_MAX = 5;

static const char* TAG = "lb_wifi";

static EventGroupHandle_t x_wifi_event_group;
static lb_settings_wifi current_settings = {};

static esp_netif_t* sta_netif = NULL;
static esp_netif_t* ap_netif = NULL;
static wifi_mode_t current_mode = WIFI_MODE_NULL;
static uint64_t sta_fallback_deadline = 0;
static int s_retry_num = 0;

static void ap_set_dns_addr(void)
{
    if (ap_netif == NULL || sta_netif == NULL) return;

    esp_netif_dns_info_t dns;
    if (esp_netif_get_dns_info(sta_netif, ESP_NETIF_DNS_MAIN, &dns) != ESP_OK)
    {
        ESP_LOGW(TAG, "Failed to get STA DNS info");
        return;
    }

    uint8_t dhcps_offer_option = DHCPS_OFFER_DNS;

    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_netif_dhcps_stop(ap_netif));
    ESP_ERROR_CHECK(esp_netif_dhcps_option(
        ap_netif,
        ESP_NETIF_OP_SET,
        ESP_NETIF_DOMAIN_NAME_SERVER,
        &dhcps_offer_option,
        sizeof(dhcps_offer_option)
    ));
    ESP_ERROR_CHECK(esp_netif_set_dns_info(ap_netif, ESP_NETIF_DNS_MAIN, &dns));
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_netif_dhcps_start(ap_netif));

    ESP_LOGI(TAG, "AP DNS configured from STA");
}

static void wifi_event_handler(void* arg, const esp_event_base_t event_base,
                               const int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED)
    {
        const wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*)event_data;
        ESP_LOGI(TAG, "Station "MACSTR" joined, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED)
    {
        const wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*)event_data;
        ESP_LOGI(TAG, "Station "MACSTR" left, AID=%d, reason:%d",
                 MAC2STR(event->mac), event->aid, event->reason);
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        ESP_LOGI(TAG, "STA started");
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        const wifi_event_sta_disconnected_t* event = (wifi_event_sta_disconnected_t*)event_data;
        wifi_mode_t mode;
        esp_wifi_get_mode(&mode);

        if (mode == WIFI_MODE_STA || mode == WIFI_MODE_APSTA)
        {
            if (mode == WIFI_MODE_STA)
            {
                const uint64_t now = esp_timer_get_time() / 1000;
                if (sta_fallback_deadline == 0)
                {
                    sta_fallback_deadline = now + current_settings.sta_fallback_to_ap_timeout * 1000;
                }

                if (now > sta_fallback_deadline)
                {
                    xEventGroupSetBits(x_wifi_event_group, WIFI_SWITCH_TO_AP);
                    ESP_LOGI(TAG, "STA disconnected (reason:%d), switching to AP mode", event->reason);
                }
                else
                {
                    esp_wifi_connect();
                    s_retry_num++;
                    const uint64_t remaining = (sta_fallback_deadline - now) / 1000;
                    ESP_LOGI(TAG, "STA disconnected (reason:%d), retry %d, remaining %d sec", event->reason,
                             s_retry_num, (int)remaining);
                }
            }
            else
            {
                if (s_retry_num < STA_RETRY_MAX)
                {
                    esp_wifi_connect();
                    s_retry_num++;
                    ESP_LOGI(TAG, "STA disconnected (reason:%d), retry %d/%d", event->reason, s_retry_num,
                             STA_RETRY_MAX);
                }
                else
                {
                    xEventGroupSetBits(x_wifi_event_group, WIFI_FAIL_BIT);
                    ESP_LOGW(TAG, "STA connection failed after %i retries", STA_RETRY_MAX);
                }
            }
        }

        xEventGroupClearBits(x_wifi_event_group, WIFI_CONNECTED_BIT);
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        const ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
        ESP_LOGI(TAG, "STA got IP:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        sta_fallback_deadline = 0;
        xEventGroupSetBits(x_wifi_event_group, WIFI_CONNECTED_BIT);

        wifi_mode_t mode;
        esp_wifi_get_mode(&mode);
        if (mode == WIFI_MODE_APSTA)
        {
            vTaskDelay(pdMS_TO_TICKS(100));
            ap_set_dns_addr();
        }
    }
}

static void wifi_init_ap_config(const lb_settings_wifi* settings, wifi_config_t* wifi_config)
{
    memset(wifi_config, 0, sizeof(wifi_config_t));
    strncpy((char*)wifi_config->ap.ssid, settings->ap_ssid, sizeof(wifi_config->ap.ssid) - 1);
    strncpy((char*)wifi_config->ap.password, settings->ap_password, sizeof(wifi_config->ap.password) - 1);
    wifi_config->ap.ssid_len = strlen(settings->ap_ssid);
    wifi_config->ap.channel = 1;
    wifi_config->ap.max_connection = 4;
    wifi_config->ap.authmode = (strlen(settings->ap_password) == 0) ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK;
    wifi_config->ap.ssid_hidden = settings->ap_ssid_hidden;
    wifi_config->ap.pmf_cfg.required = false;
}

static void wifi_init_sta_config(const lb_settings_wifi* settings, wifi_config_t* wifi_config)
{
    memset(wifi_config, 0, sizeof(wifi_config_t));

    strncpy((char*)wifi_config->sta.ssid, settings->sta_ssid, sizeof(wifi_config->sta.ssid) - 1);
    strncpy((char*)wifi_config->sta.password, settings->sta_password, sizeof(wifi_config->sta.password) - 1);
    wifi_config->sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
    wifi_config->sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config->sta.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;
    wifi_config->sta.pmf_cfg.capable = true;
    wifi_config->sta.pmf_cfg.required = false;
    wifi_config->sta.failure_retry_cnt = 0;
}

void lb_apply_wifi_config(lb_settings_wifi settings)
{
    if (memcmp(&current_settings, &settings, sizeof(lb_settings_wifi)) == 0)
    {
        ESP_LOGI(TAG, "Wi-Fi config unchanged");
        return;
    }
    current_settings = settings;
    wifi_mode_t new_mode;
    wifi_config_t ap_config = {0};
    wifi_config_t sta_config = {0};

    if (settings.sta_enabled && settings.ap_always_enabled) new_mode = WIFI_MODE_APSTA;
    else if (settings.sta_enabled) new_mode = WIFI_MODE_STA;
    else new_mode = WIFI_MODE_AP;

    ESP_LOGI(TAG, "Configuring WiFi: mode=%d, STA=%s, AP=%s",
             new_mode, settings.sta_ssid, settings.ap_ssid);

    s_retry_num = 0;
    xEventGroupClearBits(x_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);

    esp_err_t err = esp_wifi_stop();
    if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_STARTED)
    {
        ESP_LOGE(TAG, "Failed to stop WiFi: %s", esp_err_to_name(err));
        return;
    }

    vTaskDelay(pdMS_TO_TICKS(100));

    ESP_ERROR_CHECK(esp_wifi_set_mode(new_mode));

    const bool is_ap = new_mode == WIFI_MODE_AP || new_mode == WIFI_MODE_APSTA;
    const bool is_sta = new_mode == WIFI_MODE_STA || new_mode == WIFI_MODE_APSTA;

    if (is_ap && ap_netif == NULL)
    {
        ap_netif = esp_netif_create_default_wifi_ap();
        ESP_LOGI(TAG, "Created AP netif");
    }
    else if (!is_ap && ap_netif != NULL)
    {
        esp_netif_destroy(ap_netif);
        ap_netif = NULL;
        ESP_LOGI(TAG, "Destroyed AP netif");
    }

    if (is_sta && sta_netif == NULL)
    {
        sta_netif = esp_netif_create_default_wifi_sta();
        esp_netif_set_hostname(sta_netif, CONFIG_APP_MDNS_HOSTNAME);
        ESP_LOGI(TAG, "Created STA netif");
    }
    else if (!is_sta && sta_netif != NULL)
    {
        esp_netif_destroy(sta_netif);
        sta_netif = NULL;
        ESP_LOGI(TAG, "Destroyed STA netif");
    }
    current_mode = new_mode;

    if (new_mode == WIFI_MODE_APSTA)
    {
        wifi_init_ap_config(&settings, &ap_config);
        wifi_init_sta_config(&settings, &sta_config);
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));
        ESP_LOGI(TAG, "Configured APSTA mode");
    }
    else if (new_mode == WIFI_MODE_STA)
    {
        wifi_init_sta_config(&settings, &sta_config);
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));
        ESP_LOGI(TAG, "Configured STA mode");
    }
    else
    {
        wifi_init_ap_config(&settings, &ap_config);
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
        ESP_LOGI(TAG, "Configured AP mode");
    }

    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "WiFi started");
}

static void settings_listen_task(void* _)
{
    // ReSharper disable once CppDFAEndlessLoop
    for (;;)
    {
        xEventGroupWaitBits(x_settings_event_handle, SETTINGS_UPDATED_BIT,
                            pdTRUE, pdFALSE, portMAX_DELAY);
        ESP_LOGI(TAG, "Settings updated, reconfiguring WiFi");
        xEventGroupSetBits(x_wifi_event_group, WIFI_NEW_SETTINGS);
    }
}

static void config_task(void* _)
{
    // ReSharper disable once CppDFAEndlessLoop
    for (;;)
    {
        const EventBits_t bits = xEventGroupWaitBits(x_wifi_event_group, WIFI_NEW_SETTINGS | WIFI_SWITCH_TO_AP,
                                                     pdTRUE, pdFALSE, portMAX_DELAY);
        if (bits & WIFI_NEW_SETTINGS)
        {
            const lb_settings settings = lb_settings_get();
            vTaskDelay(pdMS_TO_TICKS(500));
            lb_apply_wifi_config(settings.wifi);
        }
        else if (bits & WIFI_SWITCH_TO_AP)
        {
            vTaskDelay(pdMS_TO_TICKS(100));
            current_settings.sta_enabled = false;
            lb_apply_wifi_config(current_settings);
        }
    }
}

void lb_init_wifi(void)
{
    x_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    const wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
        &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
        &wifi_event_handler, NULL));

    lb_apply_wifi_config(lb_settings_get().wifi);

    xTaskCreate(settings_listen_task, "wifi_settings_listen_task", configMINIMAL_STACK_SIZE + 2048,
                NULL, 5, NULL);
    xTaskCreate(config_task, "wifi_config_task", configMINIMAL_STACK_SIZE + 2048,
                NULL, 5, NULL);

    ESP_LOGI(TAG, "WiFi initialization complete");
}
