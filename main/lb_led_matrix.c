#include "lb_led_matrix.h"

#include <esp_log.h>
#include <led_strip.h>
#include <string.h>
#include <driver/i2s_common.h>
#include <sys/stat.h>

#include "esp_littlefs.h"

const int lb_led_matrix_width = LB_MATRIX_COLUMNS;
const int lb_led_matrix_height = LB_MATRIX_ROWS;

static const int width = lb_led_matrix_width;
static const int height = lb_led_matrix_height;
static const float brightness_multiplier = (float)0.6;

static lb_led_matrix_state m;
static led_strip_handle_t led_strip;


static const char* TAG = "lb_led_matrix";

uint8_t lb_led_matrix_get_brightness()
{
    return m.brightness;
}

void lb_led_matrix_set_brightness(const uint8_t brightness)
{
    if (brightness > 100) m.brightness = 100;
    else m.brightness = brightness;
}

int lb_led_matrix_map_pixel(int i, int j)
{
    if (i < 0 || i >= height || j < 0 || j >= width) return -1;

    i = height - 1 - i;
    j = width - 1 - j;

    const int col_base = j * height;
    if (j % 2 == 0)
    {
        return col_base + i;
    }

    return col_base + (height - 1 - i);
}

void lb_led_matrix_init()
{
    ESP_LOGI(TAG, "sizeof(lb_led_matrix) = %d", sizeof(lb_led_matrix));

    // LED strip general initialization, according to your LED board design
    const led_strip_config_t strip_config = {
        .strip_gpio_num = CONFIG_APP_LED_STRIP_GPIO, // The GPIO that connected to the LED strip's data line
        .max_leds = LB_MATRIX_LED_NUM, // The number of LEDs in the strip,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB, // Pixel format of your LED strip
        .led_model = LED_MODEL_WS2812, // LED strip model
        .flags.invert_out = false, // whether to invert the output signal
    };

    // LED strip backend configuration: RMT
    const led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT, // different clock source can lead to different power consumption
        .resolution_hz = 10 * 1000 * 1000, // RMT counter clock frequency - 10MHz
        .mem_block_symbols = 64,
        .flags.with_dma = false, // DMA feature is available on ESP target like ESP32-S3
    };

    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    ESP_LOGI(TAG, "Created LED strip object with RMT backend");

    memset(&m, 0x00, sizeof(lb_led_matrix));
    m.brightness = 100;
    lb_led_matrix_load_state();
    lb_led_matrix_apply_buffer();
}

void lb_led_matrix_destroy()
{
    ESP_ERROR_CHECK(led_strip_del(led_strip));
}

void lb_led_matrix_set_pixel_buffer(
    const int i, const int j,
    const uint8_t r, const uint8_t g, const uint8_t b
)
{
    const int k = lb_led_matrix_map_pixel(i, j);
    if (k == -1) return;

    m.matrix.m[i][j].r = r;
    m.matrix.m[i][j].g = g;
    m.matrix.m[i][j].b = b;
}

lb_led_pixel* lb_led_matrix_get_pixel_buffer(const int i, const int j)
{
    const int k = lb_led_matrix_map_pixel(i, j);
    if (k == -1) return NULL;

    return &m.matrix.m[i][j];
}

const lb_led_matrix* lb_led_matrix_get_matrix()
{
    return &m.matrix;
}

void lb_led_matrix_set_matrix(const lb_led_matrix* matrix)
{
    memcpy(m.matrix.m, matrix->m, sizeof(lb_led_pixel) * LB_MATRIX_ROWS * LB_MATRIX_COLUMNS);
}

void lb_led_matrix_apply_buffer()
{
    const float brightness = (float)m.brightness / (float)100.0 * brightness_multiplier;
    for (int i = 0; i < height; i++)
    {
        for (int j = 0; j < width; j++)
        {
            const int k = lb_led_matrix_map_pixel(i, j);
            if (k == -1) continue;
            ESP_ERROR_CHECK(
                led_strip_set_pixel(led_strip, k, m.matrix.m[i][j].r * brightness, m.matrix.m[i][j].g * brightness,
                    m.matrix.m[i][j].b * brightness)
            );
        }
    }
    ESP_ERROR_CHECK(led_strip_refresh(led_strip));
}

static esp_err_t lb_led_matrix_save_any(const void* matrix, const size_t size, const char* path)
{
    FILE* f = fopen(path, "wb");
    if (!f)
    {
        ESP_LOGE(TAG, "Failed to open %s for writing", path);
        return ESP_FAIL;
    }

    const size_t written = fwrite(matrix, size, 1, f);
    fclose(f);
    if (written != 1)
    {
        ESP_LOGE(TAG, "Failed to write matrix to file");
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t lb_led_matrix_load_any(void* matrix, const size_t size, const char* path)
{
    FILE* f = fopen(path, "rb");
    if (!f)
    {
        ESP_LOGE(TAG, "Failed to open %s for reading", path);
        return ESP_FAIL;
    }

    const size_t read = fread(matrix, size, 1, f);
    fclose(f);

    if (read != 1)
    {
        ESP_LOGE(TAG, "Failed to read matrix from file");
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t lb_led_matrix_save(const lb_led_matrix* matrix, const char* path)
{
    return lb_led_matrix_save_any(matrix, sizeof(lb_led_matrix), path);
}

esp_err_t lb_led_matrix_load(lb_led_matrix* matrix, const char* path)
{
    return lb_led_matrix_load_any(matrix, sizeof(lb_led_matrix), path);
}

void lb_led_matrix_save_state()
{
    lb_led_matrix_save_any(&m, sizeof(m), "/lfs/state.matrix");
}

void lb_led_matrix_load_state()
{
    lb_led_matrix_load_any(&m, sizeof(m), "/lfs/state.matrix");
}
