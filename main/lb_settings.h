#ifndef LB_SETTINGS_H
#define LB_SETTINGS_H

#include <stdbool.h>
#include <stdint.h>

#include "esp_bit_defs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#define LB_SETTINGS_MAX_SSID_LEN 32
#define LB_SETTINGS_MAX_PASSWORD_LEN 64
#define LB_SETTINGS_MAX_HOSTNAME_LEN 32
#define LB_SETTINGS_MAX_DEVICE_NAME_LEN 32

#define SETTINGS_UPDATED_BIT BIT0
extern EventGroupHandle_t x_settings_event_handle;

typedef struct
{
    uint32_t sta_fallback_to_ap_timeout;
    char sta_ssid[LB_SETTINGS_MAX_SSID_LEN + 1];
    char sta_password[LB_SETTINGS_MAX_PASSWORD_LEN + 1];
    bool sta_enabled;
    bool ap_always_enabled;
    char ap_ssid[LB_SETTINGS_MAX_SSID_LEN + 1];
    bool ap_ssid_hidden;
    char ap_password[LB_SETTINGS_MAX_PASSWORD_LEN + 1];
} lb_settings_wifi;

typedef struct
{
    char hostname[LB_SETTINGS_MAX_HOSTNAME_LEN + 1];
    char device_name[LB_SETTINGS_MAX_DEVICE_NAME_LEN + 1];
} lb_settings_mdns;

typedef struct
{
    uint16_t version;
    lb_settings_wifi wifi;
    lb_settings_mdns mdns;
} lb_settings;

void lb_settings_init();

int lb_settings_load_from_json(const char* json_str, int buffer_size);

int lb_settings_merge_from_json(const char* json_str, int buffer_size);

char* lb_settings_to_json();

lb_settings lb_settings_get();

void lb_settings_deinit();

#endif //LB_SETTINGS_H
