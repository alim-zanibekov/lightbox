#include "lb_gallery.h"

#include <dirent.h>
#include <sys/param.h>
#include <sys/stat.h>

#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"


#define PATH "/lfs/gallery"

const int lb_gallery_capacity = LB_GALLERY_CAPACITY;

static const char* TAG = "lb_gallery";
static int gallery[LB_GALLERY_CAPACITY + 1];

static size_t gallery_len = 0;
static int gallery_max = 0;

static SemaphoreHandle_t x_gallery_lock = NULL;

static void x_ota_lock_verify()
{
    if (unlikely(x_gallery_lock == NULL))
    {
        ESP_LOGE(TAG, "Gallery read/write mutex is NULL!");
        abort();
    }
}

int compare_ints(const void* a, const void* b)
{
    return *(const int*)a - *(const int*)b;
}

void lb_gallery_init()
{
    x_gallery_lock = xSemaphoreCreateMutex();
    if (unlikely(x_gallery_lock == NULL))
    {
        ESP_LOGE(TAG, "Failed to create gallery read/write mutex!");
        abort();
    }

    mkdir(PATH, 0777);
    DIR* dir = opendir(PATH);
    if (!dir)
    {
        ESP_LOGE(TAG, "Failed to open directory: %s", PATH);
        abort();
    }

    gallery_len = 0;
    gallery_max = 0;
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL && gallery_len < LB_GALLERY_CAPACITY)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        const long idx = strtol(entry->d_name, NULL, 10);
        gallery[gallery_len++] = (int)idx;
        gallery_max = MAX((int)idx, gallery_max);
    }
    closedir(dir);

    qsort(gallery, gallery_len, sizeof(int), compare_ints);
}

esp_err_t lb_gallery_add_image(const lb_led_matrix* matrix)
{
    x_ota_lock_verify();

    if (gallery_max >= LB_GALLERY_CAPACITY)
    {
        return ESP_FAIL;
    }

    xSemaphoreTake(x_gallery_lock, portMAX_DELAY);
    char file_path[128];
    snprintf(file_path, sizeof(file_path), "%s/%d.matrix", PATH, gallery_max + 1);
    const esp_err_t result = lb_led_matrix_save(matrix, file_path);
    if (result == ESP_OK)
    {
        gallery_max++;
        gallery[gallery_len++] = gallery_max;
    }
    xSemaphoreGive(x_gallery_lock);

    return result;
}

esp_err_t lb_gallery_delete_image(const int i)
{
    x_ota_lock_verify();
    xSemaphoreTake(x_gallery_lock, portMAX_DELAY);
    if (i < 0 || i >= gallery_len)
    {
        xSemaphoreGive(x_gallery_lock);
        return ESP_ERR_INVALID_ARG;
    }

    char file_path[128];
    snprintf(file_path, sizeof(file_path), "%s/%d.matrix", PATH, gallery[i]);
    const int res = remove(file_path);
    if (res != 0)
    {
        ESP_LOGE(TAG, "Failed to delete file: %s", file_path);
    }
    else
    {
        ESP_LOGI(TAG, "Deleted file: %s", file_path);
        for (int j = i; j < gallery_len - 1; j++) gallery[j] = gallery[j + 1];
        gallery_len--;
    }
    xSemaphoreGive(x_gallery_lock);
    if (res != 0) return ESP_FAIL;
    return ESP_OK;
}

esp_err_t lb_gallery_delete_all()
{
    x_ota_lock_verify();
    xSemaphoreTake(x_gallery_lock, portMAX_DELAY);

    DIR* dir = opendir(PATH);
    if (!dir)
    {
        ESP_LOGE(TAG, "Failed to open gallery directory: %s", PATH);
        xSemaphoreGive(x_gallery_lock);
        return ESP_FAIL;
    }

    struct dirent* entry;
    esp_err_t status = ESP_OK;

    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

        char file_path[256 + sizeof(PATH) + 1];
        snprintf(file_path, sizeof(file_path), "%s/%s", PATH, entry->d_name);

        if (remove(file_path) != 0)
        {
            ESP_LOGE(TAG, "Failed to delete file: %s", file_path);
            status = ESP_FAIL;
        }
        else
            ESP_LOGI(TAG, "Deleted file: %s", file_path);
    }

    closedir(dir);

    gallery_len = 0;
    gallery_max = 0;
    memset(gallery, 0, sizeof(gallery));

    xSemaphoreGive(x_gallery_lock);
    return status;
}


esp_err_t lb_gallery_read(const int i, lb_led_matrix* matrix)
{
    x_ota_lock_verify();
    xSemaphoreTake(x_gallery_lock, portMAX_DELAY);
    if (i < 0 || i >= gallery_len)
    {
        xSemaphoreGive(x_gallery_lock);
        return ESP_ERR_INVALID_ARG;
    }
    char file_path[128];
    snprintf(file_path, sizeof(file_path), "%s/%d.matrix", PATH, gallery[i]);
    const esp_err_t result = lb_led_matrix_load(matrix, file_path);
    xSemaphoreGive(x_gallery_lock);
    return result;
}

size_t lb_gallery_len()
{
    return gallery_len;
}
