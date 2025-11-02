#include "lb_ota.h"

#include <sys/errno.h>

#include "esp_app_format.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

#define BUFF_SIZE 1024
#define HASH_LEN 32

const char* TAG = "lb_ota";
static char ota_write_data[BUFF_SIZE + 1] = {0};
static SemaphoreHandle_t x_ota_lock = NULL;

static void x_ota_lock_verify()
{
    if (unlikely(x_ota_lock == NULL))
    {
        ESP_LOGE(TAG, "OTA mutex is NULL!");
        abort();
    }
}

static void restart_timer_callback(TimerHandle_t xTimer)
{
    esp_restart();
}

static void esp_async_restart_delayed(const uint32_t delay_ms)
{
    TimerHandle_t timer = xTimerCreate(
        "restart_timer", pdMS_TO_TICKS(delay_ms), pdFALSE, NULL, restart_timer_callback
    );
    if (likely(timer != NULL))
        xTimerStart(timer, 0);
    else
        esp_restart();
}

static void print_sha256(const uint8_t* image_hash, const char* label)
{
    char hash_print[HASH_LEN * 2 + 1];
    hash_print[HASH_LEN * 2] = 0;
    for (int i = 0; i < HASH_LEN; ++i)
    {
        sprintf(&hash_print[i * 2], "%02x", image_hash[i]);
    }
    ESP_LOGI(TAG, "%s: %s", label, hash_print);
}

void lb_ota_init()
{
    x_ota_lock = xSemaphoreCreateMutex();
    if (unlikely(x_ota_lock == NULL))
    {
        ESP_LOGE(TAG, "Failed to create OTA mutex!");
        abort();
    }

    ESP_LOGI(TAG, "Initializing OTA");

    uint8_t sha_256[HASH_LEN] = {0};
    esp_partition_t partition;

    partition.address = ESP_PARTITION_TABLE_OFFSET;
    partition.size = ESP_PARTITION_TABLE_MAX_LEN;
    partition.type = ESP_PARTITION_TYPE_DATA;
    esp_partition_get_sha256(&partition, sha_256);
    print_sha256(sha_256, "SHA-256 for the partition table");

    partition.address = ESP_BOOTLOADER_OFFSET;
    partition.size = ESP_PARTITION_TABLE_OFFSET;
    partition.type = ESP_PARTITION_TYPE_APP;
    esp_partition_get_sha256(&partition, sha_256);
    print_sha256(sha_256, "SHA-256 for bootloader");

    esp_partition_get_sha256(esp_ota_get_running_partition(), sha_256);
    print_sha256(sha_256, "SHA-256 for current firmware");

    const esp_partition_t* running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;
    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK)
    {
        if (ota_state == ESP_OTA_IMG_PENDING_VERIFY)
        {
            esp_ota_mark_app_valid_cancel_rollback();
            // esp_ota_mark_app_invalid_rollback_and_reboot(); // Use this for rollback
        }
    }
}

static void send_response_error(httpd_req_t* req, const char* status, const char* error)
{
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_status(req, status);
    char buffer[128];
    snprintf(buffer, sizeof(buffer), "{\"error\": \"%s\"}", error);
    httpd_resp_sendstr(req, buffer);
}

static esp_err_t lb_ota_update_internal(httpd_req_t* req)
{
    esp_err_t err;
    esp_ota_handle_t update_handle = 0;

    const esp_partition_t* update_partition = NULL;

    ESP_LOGI(TAG, "Starting OTA task");

    const esp_partition_t* configured = esp_ota_get_boot_partition();
    const esp_partition_t* running = esp_ota_get_running_partition();

    if (configured != running)
    {
        ESP_LOGW(TAG, "Configured OTA boot partition at offset 0x%08"PRIx32", but running from offset 0x%08"PRIx32,
                 configured->address, running->address);
        ESP_LOGW(TAG, "(This can happen if either the OTA boot data or preferred boot image become corrupted somehow)");
    }
    ESP_LOGI(TAG, "Running partition type %d subtype %d (offset 0x%08"PRIx32")",
             running->type, running->subtype, running->address);


    update_partition = esp_ota_get_next_update_partition(NULL);
    assert(update_partition != NULL);
    ESP_LOGI(TAG, "Writing to partition subtype %d at offset 0x%"PRIx32,
             update_partition->subtype, update_partition->address);

    int binary_file_length = 0;

    bool image_header_was_checked = false;
    while (1)
    {
        const int data_read = httpd_req_recv(req, (char*)ota_write_data, BUFF_SIZE);
        if (data_read < 0)
        {
            ESP_LOGE(TAG, "Error receiving firmware (%s)", esp_err_to_name(data_read));
            if (update_handle != 0)
            {
                esp_ota_abort(update_handle);
            }
            send_response_error(req, "400", "Error receiving firmware");
            return ESP_FAIL;
        }
        else if (data_read > 0)
        {
            if (image_header_was_checked == false)
            {
                esp_app_desc_t new_app_info;
                if (data_read > sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(
                    esp_app_desc_t))
                {
                    memcpy(&new_app_info,
                           &ota_write_data[sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t)],
                           sizeof(esp_app_desc_t));
                    ESP_LOGI(TAG, "New firmware version: %s", new_app_info.version);

                    esp_app_desc_t running_app_info;
                    if (esp_ota_get_partition_description(running, &running_app_info) == ESP_OK)
                    {
                        ESP_LOGI(TAG, "Running firmware version: %s", running_app_info.version);
                    }

                    const esp_partition_t* last_invalid_app = esp_ota_get_last_invalid_partition();
                    esp_app_desc_t invalid_app_info;
                    if (esp_ota_get_partition_description(last_invalid_app, &invalid_app_info) == ESP_OK)
                    {
                        ESP_LOGI(TAG, "Last invalid firmware version: %s", invalid_app_info.version);
                    }

                    // check current version with last invalid partition
                    if (last_invalid_app != NULL)
                    {
                        if (memcmp(invalid_app_info.version, new_app_info.version, sizeof(new_app_info.version)) == 0)
                        {
                            ESP_LOGW(TAG, "New version is the same as invalid version.");
                            ESP_LOGW(
                                TAG,
                                "Previously, there was an attempt to launch the firmware with %s version, but it failed.",
                                invalid_app_info.version);
                            ESP_LOGW(TAG, "The firmware has been rolled back to the previous version.");
                            send_response_error(req, "400", "Firmware version is the same as invalid version");
                            return ESP_FAIL;
                        }
                    }
#ifndef CONFIG_APP_OTA_SKIP_VERSION_CHECK
                    if (memcmp(new_app_info.version, running_app_info.version, sizeof(new_app_info.version)) == 0)
                    {
                        ESP_LOGW(TAG, "Current   running version is the same as a new. We will not continue the update");
                        send_response_error(
                            req, "400",
                            "Current running version is the same as a new. Update aborted"
                        );
                        return ESP_FAIL;
                    }
#endif

                    image_header_was_checked = true;

                    err = esp_ota_begin(update_partition, OTA_WITH_SEQUENTIAL_WRITES, &update_handle);
                    if (err != ESP_OK)
                    {
                        ESP_LOGE(TAG, "esp_ota_begin failed (%s)", esp_err_to_name(err));
                        esp_ota_abort(update_handle);
                        send_response_error(req, "500", "esp_ota_begin failed");
                        return ESP_FAIL;
                    }
                    ESP_LOGI(TAG, "esp_ota_begin succeeded");
                }
                else
                {
                    ESP_LOGE(TAG, "received package is not fit len");
                    esp_ota_abort(update_handle);
                    send_response_error(req, "500", "received package is not fit len");
                    return ESP_FAIL;
                }
            }
            err = esp_ota_write(update_handle, (const void*)ota_write_data, data_read);
            if (err != ESP_OK)
            {
                esp_ota_abort(update_handle);
                send_response_error(req, "500", "esp_ota_write error");
                return ESP_FAIL;
            }
            binary_file_length += data_read;
            ESP_LOGD(TAG, "Written image length %d", binary_file_length);
            if (binary_file_length == req->content_len)
            {
                ESP_LOGI(TAG, "Firmware receive completed");
                break;
            }
        }
        else //if (data_read == 0)
        {
            if (errno == ECONNRESET || errno == ENOTCONN)
            {
                ESP_LOGE(TAG, "Connection closed, errno = %d", errno);
                if (update_handle != 0) esp_ota_abort(update_handle);
                return ESP_FAIL;
            }
            if (binary_file_length == req->content_len) break;
        }
    }

    if (update_handle == 0)
    {
        send_response_error(req, "400", "No firmware provided");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Total Write binary data length: %d", binary_file_length);

    err = esp_ota_end(update_handle);
    if (err != ESP_OK)
    {
        if (err == ESP_ERR_OTA_VALIDATE_FAILED)
        {
            ESP_LOGE(TAG, "Image validation failed, image is corrupted");
            send_response_error(req, "400", "Image validation failed, image is corrupted");
        }
        else
        {
            ESP_LOGE(TAG, "esp_ota_end failed (%s)!", esp_err_to_name(err));
            send_response_error(req, "400", "esp_ota_end failed");
        }
        return ESP_FAIL;
    }

    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed (%s)!", esp_err_to_name(err));
        send_response_error(req, "500", "esp_ota_set_boot_partition failed");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Prepare to restart system!");

    esp_async_restart_delayed(500);

    return ESP_OK;
}

esp_err_t lb_ota_update(httpd_req_t* req)
{
    x_ota_lock_verify();
    xSemaphoreTake(x_ota_lock, portMAX_DELAY);
    const esp_err_t result = lb_ota_update_internal(req);
    xSemaphoreGive(x_ota_lock);
    return result;
}
