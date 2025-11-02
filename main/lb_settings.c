#include "lb_settings.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "cJSON.h"
#include "esp_log.h"
#include "freertos/semphr.h"
#include "nvs.h"

#define LB_SETTINGS_NVS_NAMESPACE "lightbox"
#define LB_SETTINGS_NVS_KEY "settings"

#define LB_SETTINGS_VERSION 1

static const char* TAG = "lb_settings";

static lb_settings s_settings;
static SemaphoreHandle_t x_settings_lock = NULL;
EventGroupHandle_t x_settings_event_handle = NULL;

static const lb_settings s_default_settings = {
    .version = LB_SETTINGS_VERSION,
    .wifi = {
        .sta_ssid = CONFIG_APP_WIFI_STA_SSID,
        .sta_password = CONFIG_APP_WIFI_STA_PASSWORD,
#ifdef CONFIG_APP_WIFI_STA_ENABLED
        .sta_enabled = true,
#else
        .sta_enabled = false,
#endif
        .sta_fallback_to_ap_timeout = CONFIG_APP_WIFI_STA_TO_AP_FALLBACK_TIMEOUT,

        .ap_ssid = CONFIG_APP_WIFI_AP_SSID,
        .ap_password = CONFIG_APP_WIFI_AP_PASSWORD,
#ifdef CONFIG_APP_WIFI_AP_SSID_HIDDEN
        .ap_ssid_hidden = true,
#else
        .ap_ssid_hidden = false,
#endif
#ifdef CONFIG_APP_WIFI_AP_ALWAYS_ENABLED
        .ap_always_enabled = true,
#else
        .ap_always_enabled = false,
#endif
    },
    .mdns = {
        .hostname = CONFIG_APP_MDNS_HOSTNAME,
        .device_name = CONFIG_APP_DEVICE_NAME,
    }
};


static void x_settings_lock_verify()
{
    if (unlikely(x_settings_lock == NULL))
    {
        ESP_LOGE(TAG, "Settings read/write mutex is NULL!");
        abort();
    }
}

static int lb_settings_save_to_nvs()
{
    x_settings_lock_verify();

    const lb_settings tmp_settings = s_settings;

    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(LB_SETTINGS_NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open NVS handle: %s", esp_err_to_name(err));
        return ESP_FAIL;
    }

    err = nvs_set_blob(nvs_handle, LB_SETTINGS_NVS_KEY, &tmp_settings, sizeof(lb_settings));
    if (err != ESP_OK)
    {
        nvs_close(nvs_handle);
        ESP_LOGE(TAG, "Failed to write settings to NVS: %s", esp_err_to_name(err));
        return ESP_FAIL;
    }
    err = nvs_commit(nvs_handle);
    if (err != ESP_OK)
    {
        nvs_close(nvs_handle);
        ESP_LOGE(TAG, "Failed to commit NVS changes: %s", esp_err_to_name(err));
        return ESP_FAIL;
    }
    nvs_close(nvs_handle);
    return ESP_OK;
}

static int lb_settings_erase_nvs();

static int lb_settings_load_from_nvs()
{
    x_settings_lock_verify();

    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(LB_SETTINGS_NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK)
    {
        if (err == ESP_ERR_NVS_NOT_FOUND)
        {
            ESP_LOGI(TAG, "Settings namespace not found in NVS");
            return ESP_ERR_NOT_FOUND;
        }
        ESP_LOGE(TAG, "Failed to open NVS handle: %s", esp_err_to_name(err));
        return ESP_FAIL;
    }

    size_t required_size = 0;
    err = nvs_get_blob(nvs_handle, LB_SETTINGS_NVS_KEY, NULL, &required_size);
    if (err != ESP_OK)
    {
        if (err == ESP_ERR_NVS_NOT_FOUND)
        {
            ESP_LOGI(TAG, "Settings key not found in NVS");
            nvs_close(nvs_handle);
            return ESP_ERR_NOT_FOUND;
        }
        ESP_LOGE(TAG, "Failed to get settings size from NVS: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return ESP_FAIL;
    }

    if (required_size != sizeof(lb_settings))
    {
        ESP_LOGE(TAG, "Settings size mismatch: expected %d, got %d", sizeof(lb_settings), required_size);
        nvs_close(nvs_handle);
        ESP_LOGI(TAG, "Deleting old settings entry");
        lb_settings_erase_nvs();
        return ESP_FAIL;
    }

    lb_settings loaded_settings;
    err = nvs_get_blob(nvs_handle, LB_SETTINGS_NVS_KEY, &loaded_settings, &required_size);
    nvs_close(nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to read settings from NVS: %s", esp_err_to_name(err));
        return ESP_FAIL;
    }

    if (loaded_settings.version != LB_SETTINGS_VERSION)
    {
        ESP_LOGW(TAG, "Settings version mismatch: expected %d, got %d", LB_SETTINGS_VERSION, loaded_settings.version);
        return ESP_FAIL;
    }

    s_settings = loaded_settings;
    return ESP_OK;
}

static int lb_settings_erase_nvs()
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(LB_SETTINGS_NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open NVS handle: %s", esp_err_to_name(err));
        return ESP_FAIL;
    }
    err = nvs_erase_key(nvs_handle, LB_SETTINGS_NVS_KEY);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGE(TAG, "Failed to erase settings from NVS: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return ESP_FAIL;
    }
    err = nvs_commit(nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to commit NVS changes: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return ESP_FAIL;
    }
    nvs_close(nvs_handle);
    return ESP_OK;
}

void lb_settings_init()
{
    if (x_settings_lock != NULL) return;

    s_settings = s_default_settings;

    x_settings_event_handle = xEventGroupCreate();
    if (unlikely(x_settings_event_handle == NULL))
    {
        ESP_LOGE(TAG, "Failed to create settings event group!");
        abort();
    }

    x_settings_lock = xSemaphoreCreateMutex();
    if (unlikely(x_settings_lock == NULL))
    {
        ESP_LOGE(TAG, "Failed to create settings read/write mutex!");
        abort();
    }

    xSemaphoreTake(x_settings_lock, portMAX_DELAY);
    lb_settings_load_from_nvs();
    xSemaphoreGive(x_settings_lock);
}

void lb_settings_deinit(void)
{
    if (x_settings_lock != NULL)
    {
        vSemaphoreDelete(x_settings_lock);
        x_settings_lock = NULL;
    }
    if (x_settings_event_handle != NULL)
    {
        vEventGroupDelete(x_settings_event_handle);
        x_settings_event_handle = NULL;
    }
}

static int parse_settings_from_json(const cJSON* json, lb_settings* settings, const bool partial)
{
    if (settings == NULL || json == NULL) return ESP_FAIL;

    const cJSON* item = NULL;
    item = cJSON_GetObjectItem(json, "staSsid");
    if (item != NULL && cJSON_IsString(item))
    {
        strncpy(settings->wifi.sta_ssid, item->valuestring, LB_SETTINGS_MAX_SSID_LEN);
        settings->wifi.sta_ssid[LB_SETTINGS_MAX_SSID_LEN] = '\0';
    }
    else if (!partial) return ESP_FAIL;

    item = cJSON_GetObjectItem(json, "staPassword");
    if (item != NULL && cJSON_IsString(item))
    {
        strncpy(settings->wifi.sta_password, item->valuestring, LB_SETTINGS_MAX_PASSWORD_LEN);
        settings->wifi.sta_password[LB_SETTINGS_MAX_PASSWORD_LEN] = '\0';
    }
    else if (!partial) return ESP_FAIL;

    item = cJSON_GetObjectItem(json, "staEnabled");
    if (item != NULL && cJSON_IsBool(item)) settings->wifi.sta_enabled = cJSON_IsTrue(item);
    else if (!partial) return ESP_FAIL;

    item = cJSON_GetObjectItem(json, "staFallbackToApTimeout");
    if (item != NULL && cJSON_IsNumber(item)) settings->wifi.sta_fallback_to_ap_timeout = (uint32_t)item->valuedouble;
    else if (!partial) return ESP_FAIL;

    item = cJSON_GetObjectItem(json, "apSsid");
    if (item != NULL && cJSON_IsString(item))
    {
        strncpy(settings->wifi.ap_ssid, item->valuestring, LB_SETTINGS_MAX_SSID_LEN);
        settings->wifi.ap_ssid[LB_SETTINGS_MAX_SSID_LEN] = '\0';
    }
    else if (!partial) return ESP_FAIL;

    item = cJSON_GetObjectItem(json, "apPassword");
    if (item != NULL && cJSON_IsString(item))
    {
        strncpy(settings->wifi.ap_password, item->valuestring, LB_SETTINGS_MAX_PASSWORD_LEN);
        settings->wifi.ap_password[LB_SETTINGS_MAX_PASSWORD_LEN] = '\0';
    }
    else if (!partial) return ESP_FAIL;

    item = cJSON_GetObjectItem(json, "apAlwaysEnabled");
    if (item != NULL && cJSON_IsBool(item)) settings->wifi.ap_always_enabled = cJSON_IsTrue(item);
    else if (!partial) return ESP_FAIL;

    item = cJSON_GetObjectItem(json, "apSsidHidden");
    if (item != NULL && cJSON_IsBool(item)) settings->wifi.ap_ssid_hidden = cJSON_IsTrue(item);
    else if (!partial) return ESP_FAIL;

    item = cJSON_GetObjectItem(json, "hostname");
    if (item != NULL && cJSON_IsString(item))
    {
        strncpy(settings->mdns.hostname, item->valuestring, LB_SETTINGS_MAX_HOSTNAME_LEN);
        settings->mdns.hostname[LB_SETTINGS_MAX_HOSTNAME_LEN] = '\0';
    }
    else if (!partial) return ESP_FAIL;

    item = cJSON_GetObjectItem(json, "deviceName");
    if (item != NULL && cJSON_IsString(item))
    {
        strncpy(settings->mdns.device_name, item->valuestring, LB_SETTINGS_MAX_DEVICE_NAME_LEN);
        settings->mdns.device_name[LB_SETTINGS_MAX_DEVICE_NAME_LEN] = '\0';
    }
    else if (!partial) return ESP_FAIL;

    return ESP_OK;
}

int lb_settings_load_from_json(const char* json_str, const int buffer_size)
{
    x_settings_lock_verify();
    if (json_str == NULL) return ESP_FAIL;

    cJSON* json = cJSON_ParseWithLength(json_str, buffer_size);
    if (json == NULL) return ESP_FAIL;

    lb_settings tmp_settings = {0};
    int result = parse_settings_from_json(json, &tmp_settings, false);
    if (result == 0)
    {
        xSemaphoreTake(x_settings_lock, portMAX_DELAY);

        memcpy(&s_settings, &tmp_settings, sizeof(lb_settings));
        result = lb_settings_save_to_nvs();

        xSemaphoreGive(x_settings_lock);
    }
    cJSON_Delete(json);

    if (result == 0)
    {
        xEventGroupSetBits(x_settings_event_handle, SETTINGS_UPDATED_BIT);
    }

    return result;
}

int lb_settings_merge_from_json(const char* json_str, const int buffer_size)
{
    x_settings_lock_verify();
    if (json_str == NULL) return ESP_FAIL;

    cJSON* json = cJSON_Parse(json_str);
    if (json == NULL) return ESP_FAIL;

    xSemaphoreTake(x_settings_lock, portMAX_DELAY);
    lb_settings tmp_settings = s_settings;
    xSemaphoreGive(x_settings_lock);

    int result = parse_settings_from_json(json, &tmp_settings, true);
    if (result == 0)
    {
        xSemaphoreTake(x_settings_lock, portMAX_DELAY);

        memcpy(&s_settings, &tmp_settings, sizeof(lb_settings));
        result = lb_settings_save_to_nvs();

        xSemaphoreGive(x_settings_lock);
    }
    cJSON_Delete(json);

    if (result == 0)
    {
        xEventGroupSetBits(x_settings_event_handle, SETTINGS_UPDATED_BIT);
    }

    return result;
}

char* lb_settings_to_json(void)
{
    x_settings_lock_verify();

    xSemaphoreTake(x_settings_lock, portMAX_DELAY);
    const lb_settings s = s_settings;
    xSemaphoreGive(x_settings_lock);

    cJSON* root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "staFallbackToApTimeout", (double)s.wifi.sta_fallback_to_ap_timeout);
    cJSON_AddStringToObject(root, "staSsid", s.wifi.sta_ssid);
    cJSON_AddStringToObject(root, "staPassword", s.wifi.sta_password);
    cJSON_AddBoolToObject(root, "staEnabled", s.wifi.sta_enabled);
    cJSON_AddBoolToObject(root, "apAlwaysEnabled", s.wifi.ap_always_enabled);
    cJSON_AddStringToObject(root, "apSsid", s.wifi.ap_ssid);
    cJSON_AddBoolToObject(root, "apSsidHidden", s.wifi.ap_ssid_hidden);
    cJSON_AddStringToObject(root, "apPassword", s.wifi.ap_password);
    cJSON_AddStringToObject(root, "hostname", s.mdns.hostname);
    cJSON_AddStringToObject(root, "deviceName", s.mdns.device_name);

    char* json_str = cJSON_Print(root);
    if (json_str == NULL) return NULL;
    cJSON_Delete(root);

    return json_str;
}

lb_settings lb_settings_get()
{
    x_settings_lock_verify();

    lb_settings out_settings;
    xSemaphoreTake(x_settings_lock, portMAX_DELAY);
    memcpy(&out_settings, &s_settings, sizeof(lb_settings));
    xSemaphoreGive(x_settings_lock);

    return out_settings;
}
