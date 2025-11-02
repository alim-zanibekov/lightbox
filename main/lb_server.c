#include "lb_server.h"

#include <esp_http_server.h>
#include <stdint.h>
#include <esp_log.h>
#include <esp_vfs.h>
#include <sys/param.h>

#include "cJSON.h"
#include "esp_ota_ops.h"
#include "lb_gallery.h"
#include "lb_led_matrix.h"
#include "lb_settings.h"
#include "lb_ota.h"

#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + CONFIG_SPIFFS_OBJ_NAME_LEN)
#define HTTP_QUERY_MAX_LEN 64
#define HTTP_QUERY_VALUE_MAX_LEN 64
#define HTTP_CONTENT_TYPE_MAX_LEN 32

static const char* TAG = "lb_server";

typedef struct
{
    httpd_handle_t server;
    SemaphoreHandle_t x_LED_write_lock;
    SemaphoreHandle_t x_gallery_write_lock;
} server_ctx;


static char* get_path_from_uri(const char* base_path, const char* uri, char* dest, const size_t dest_size)
{
    const size_t base_pathlen = strlen(base_path);
    size_t pathlen = strlen(uri);

    const char* query = strchr(uri, '?');
    if (query)
    {
        pathlen = MIN(pathlen, query - uri);
    }

    const char* hash = strchr(uri, '#');
    if (hash)
    {
        pathlen = MIN(pathlen, hash - uri);
    }

    if (base_pathlen + pathlen + 1 > dest_size) // Full path string won't fit into the destination buffer
    {
        return NULL;
    }
    strcpy(dest, base_path);
    strlcpy(dest + base_pathlen, uri, pathlen + 1);
    return dest + base_pathlen;
}

void set_cors(httpd_req_t* req)
{
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "*");
}

esp_err_t send_response_options(httpd_req_t* req)
{
    httpd_resp_sendstr(req, "OK");
    return ESP_OK;
}


static void send_response_error(httpd_req_t* req, const char* status, const char* error)
{
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_status(req, status);
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "{\"error\": \"%s\"}", error);
    httpd_resp_sendstr(req, buffer);
}

esp_err_t send_response_405(httpd_req_t* req)
{
    send_response_error(req, "405", "Method not allowed");
    return ESP_OK;
}

static esp_err_t get_front(httpd_req_t* req)
{
    char filepath[FILE_PATH_MAX];
    char* filename = get_path_from_uri("", req->uri, filepath, sizeof(filepath));

    if (!filename)
    {
        httpd_resp_send_err(req, HTTPD_414_URI_TOO_LONG, "URI is too long");
        return ESP_FAIL;
    }

    if (filename[strlen(filename) - 1] == '/')
    {
        filename[strlen(filename) - 1] = '\0';
    }

    if (strcmp(filename, "/index.html") == 0 || filename[0] == '\0')
    {
        extern const uint8_t index_html_start[] asm("_binary_index_html_start");
        extern const uint8_t index_html_end[] asm("_binary_index_html_end");
        httpd_resp_set_type(req, "text/html");
        httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
        httpd_resp_send(req, (const char*)index_html_start, (int)(index_html_end - index_html_start));
        return ESP_OK;
    }
    if (strcmp(filename, "/favicon.svg") == 0 || filename[0] == '\0')
    {
        extern const uint8_t favicon_svg_start[] asm("_binary_favicon_svg_start");
        extern const uint8_t favicon_svg_end[] asm("_binary_favicon_svg_end");
        httpd_resp_set_type(req, "image/svg+xml");
        httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
        httpd_resp_send(req, (const char*)favicon_svg_start, (int)(favicon_svg_end - favicon_svg_start));
        return ESP_OK;
    }
    if (strcmp(filename, "/assets/index.js") == 0)
    {
        extern const uint8_t assets_index_js_start[] asm("_binary_assets_index_js_start");
        extern const uint8_t assets_index_js_end[] asm("_binary_assets_index_js_end");
        httpd_resp_set_type(req, "application/javascript");
        httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
        httpd_resp_send(req, (const char*)assets_index_js_start, (int)(assets_index_js_end - assets_index_js_start));
        return ESP_OK;
    }
    if (strcmp(filename, "/assets/index.css") == 0)
    {
        extern const uint8_t assets_index_css_start[] asm("_binary_assets_index_css_start");
        extern const uint8_t assets_index_css_end[] asm("_binary_assets_index_css_end");
        httpd_resp_set_type(req, "text/css");
        httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
        httpd_resp_send(req, (const char*)assets_index_css_start, (int)(assets_index_css_end - assets_index_css_start));
        return ESP_OK;
    }

    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Does not exist");
    return ESP_FAIL;
}

esp_err_t validate_content_type(httpd_req_t* req, const char* required_content_type)
{
    const size_t len = httpd_req_get_hdr_value_len(req, "Content-Type");
    if (len == 0)
    {
        send_response_error(req, "400", "Missing Content-Type header");
        return ESP_FAIL;
    }
    char content_type[HTTP_CONTENT_TYPE_MAX_LEN];
    if (httpd_req_get_hdr_value_str(req, "Content-Type", content_type, sizeof(content_type)) != ESP_OK)
    {
        send_response_error(req, "400", "Failed to read Content-Type header");
        return ESP_FAIL;
    }
    if (strcmp(content_type, required_content_type) != 0)
    {
        send_response_error(req, "415", "Unsupported Content-Type");
        return ESP_FAIL;
    }
    return ESP_OK;
}


static esp_err_t buffer_write_chunked(
    httpd_req_t* req,
    uint8_t* buffer,
    const size_t buffer_size,
    ssize_t* offset,
    const void* src,
    size_t len)
{
    const uint8_t* ptr = (const uint8_t*)src;

    while (len > 0)
    {
        const ssize_t space = (ssize_t)buffer_size - *offset;
        const ssize_t to_copy = len > (size_t)space ? space : (ssize_t)len;
        memcpy(buffer + *offset, ptr, to_copy);
        *offset += to_copy;
        ptr += to_copy;
        len -= to_copy;

        if (*offset == (ssize_t)buffer_size)
        {
            if (httpd_resp_send_chunk(req, (const char*)buffer, (ssize_t)buffer_size) != ESP_OK)
                return ESP_FAIL;
            *offset = 0;
        }
    }

    return ESP_OK;
}


static esp_err_t lb_led_matrix_write_binary(httpd_req_t* req, const lb_led_matrix* m, uint8_t* buffer,
                                            const size_t buffer_size, ssize_t* offset, const bool write_name)
{
    if (write_name)
    {
        const uint16_t name_len = (uint16_t)strnlen(m->name, sizeof(m->name));
        esp_err_t res = buffer_write_chunked(req, buffer, buffer_size, offset, &name_len, sizeof(name_len));
        if (res != ESP_OK) return res;
        if (name_len > 0)
        {
            res = buffer_write_chunked(req, buffer, buffer_size, offset, m->name, name_len);
            if (res != ESP_OK) return res;
        }
    }
    const size_t total_size = lb_led_matrix_width * lb_led_matrix_height * sizeof(lb_led_pixel);
    const lb_led_pixel* data = (const lb_led_pixel*)m->m;
    return buffer_write_chunked(req, buffer, buffer_size, offset, data, total_size);
}


static esp_err_t lb_led_matrix_read_binary(httpd_req_t* req, const lb_led_matrix* m)
{
    lb_led_pixel* data = (lb_led_pixel*)m->m;
    const size_t total_size = lb_led_matrix_width * lb_led_matrix_height * sizeof(lb_led_pixel);
    size_t received = 0;

    while (received < total_size)
    {
        uint8_t buffer[1024];
        const size_t chunk = total_size - received > sizeof(buffer) ? sizeof(buffer) : total_size - received;

        const int read = httpd_req_recv(req, (char*)buffer, chunk);
        if (read <= 0)
            return ESP_FAIL;

        memcpy((uint8_t*)data + received, buffer, read);
        received += read;
    }

    return ESP_OK;
}

static esp_err_t get_leds_handler_bin(httpd_req_t* req)
{
    if (req->method == HTTP_OPTIONS) return send_response_options(req);
    if (req->method != HTTP_GET) return send_response_405(req);

    httpd_resp_set_type(req, "application/octet-stream");
    set_cors(req);

    uint8_t buffer[1024];
    buffer[0] = lb_led_matrix_get_brightness();
    *(int16_t*)&buffer[1] = (int16_t)lb_led_matrix_width;
    *(int16_t*)&buffer[3] = (int16_t)lb_led_matrix_height;
    ssize_t offset = 5;

    if (lb_led_matrix_write_binary(req, lb_led_matrix_get_matrix(), buffer, sizeof(buffer), &offset, false) != ESP_OK)
        return ESP_FAIL;

    if (offset != 0 && httpd_resp_send_chunk(req, (const char*)buffer, offset) != ESP_OK)
        return ESP_FAIL;

    if (httpd_resp_send_chunk(req, NULL, 0) != ESP_OK)
        return ESP_FAIL;

    return ESP_OK;
}

static esp_err_t set_leds_handler_bin(httpd_req_t* req)
{
    set_cors(req);
    if (req->method == HTTP_OPTIONS) return send_response_options(req);
    if (req->method != HTTP_POST) return send_response_405(req);
    if (validate_content_type(req, "application/octet-stream") != ESP_OK) return ESP_OK;


    const server_ctx* ctx = req->user_ctx;

    const size_t expected = 1 + lb_led_matrix_width * lb_led_matrix_height * sizeof(lb_led_pixel);
    if (req->content_len != expected)
    {
        send_response_error(req, "400", "Invalid data length");
        return ESP_OK;
    }

    uint8_t brightness;
    if (httpd_req_recv(req, (char*)&brightness, 1) <= 0) return ESP_FAIL;

    lb_led_matrix temp_matrix = {0};
    if (lb_led_matrix_read_binary(req, &temp_matrix) != ESP_OK)
    {
        send_response_error(req, "400", "Failed to read matrix");
        return ESP_OK;
    }

    xSemaphoreTake(ctx->x_LED_write_lock, portMAX_DELAY);

    lb_led_matrix_set_brightness(brightness);
    lb_led_matrix_set_matrix(&temp_matrix);
    lb_led_matrix_apply_buffer();
    lb_led_matrix_save_state();

    xSemaphoreGive(ctx->x_LED_write_lock);

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, "{\"success\": true}");
}


static esp_err_t get_settings(httpd_req_t* req)
{
    set_cors(req);
    if (req->method == HTTP_OPTIONS) return send_response_options(req);
    if (req->method != HTTP_GET) return send_response_405(req);

    char* json_str = lb_settings_to_json();
    if (json_str == NULL)
    {
        send_response_error(req, "500", "Unable to read settings");
        return ESP_OK;
    }
    httpd_resp_set_type(req, "application/json");
    const esp_err_t result = httpd_resp_sendstr(req, json_str);
    free(json_str);
    return result;
}

static esp_err_t put_settings(httpd_req_t* req)
{
    set_cors(req);
    if (req->method == HTTP_OPTIONS) return send_response_options(req);
    if (req->method != HTTP_PUT) return send_response_405(req);
    if (validate_content_type(req, "application/json") != ESP_OK) return ESP_OK;


    char buffer[1024];
    const int read = httpd_req_recv(req, (char*)buffer, sizeof(buffer));
    if (read <= 0)
    {
        send_response_error(req, "400", "Unable to read the data");
        return ESP_OK;
    }

    const int res = lb_settings_merge_from_json(buffer, sizeof(buffer));
    if (res != ESP_OK)
    {
        send_response_error(req, "400", "Invalid data");
        return ESP_OK;
    }

    char* json_str = lb_settings_to_json();
    if (json_str == NULL)
    {
        send_response_error(req, "500", "Unable to read settings");
        return ESP_OK;
    }

    httpd_resp_set_type(req, "application/json");
    const esp_err_t result = httpd_resp_sendstr(req, json_str);
    free(json_str);
    return result;
}

static esp_err_t get_gallery(httpd_req_t* req)
{
    set_cors(req);
    if (req->method == HTTP_OPTIONS) return send_response_options(req);
    if (req->method != HTTP_GET) return send_response_405(req);

    httpd_resp_set_type(req, "application/octet-stream");

    const server_ctx* ctx = req->user_ctx;
    xSemaphoreTake(ctx->x_gallery_write_lock, portMAX_DELAY);

    const size_t buffer_size = 1024;
    uint8_t buffer[buffer_size];

    *(int16_t*)&buffer[0] = (int16_t)lb_gallery_capacity;
    *(int16_t*)&buffer[2] = (int16_t)lb_gallery_len();
    *(int16_t*)&buffer[4] = (int16_t)lb_led_matrix_width;
    *(int16_t*)&buffer[6] = (int16_t)lb_led_matrix_height;

    ssize_t offset = 8;
    lb_led_matrix* m = malloc(sizeof(lb_led_matrix));
    if (m == NULL)
    {
        xSemaphoreGive(ctx->x_gallery_write_lock);
        send_response_error(req, "500", "Unable to allocate response buffer");
        return ESP_OK;
    }

    const size_t count = lb_gallery_len();
    for (int i = 0; i < count; i++)
    {
        if (lb_gallery_read(i, m) != ESP_OK)
        {
            xSemaphoreGive(ctx->x_gallery_write_lock);
            free(m);
            send_response_error(req, "500", "Unable to read matrix");
            return ESP_OK;
        }

        const esp_err_t res = lb_led_matrix_write_binary(req, m, buffer, buffer_size, &offset, true);
        if (res != ESP_OK)
        {
            xSemaphoreGive(ctx->x_gallery_write_lock);
            free(m);
            send_response_error(req, "500", "Unable to write matrix");
            return ESP_OK;
        }
    }
    free(m);
    xSemaphoreGive(ctx->x_gallery_write_lock);

    if (offset > 0 && httpd_resp_send_chunk(req, (const char*)buffer, offset) != ESP_OK) return ESP_FAIL;
    return httpd_resp_send_chunk(req, NULL, 0);
}

static esp_err_t post_gallery(httpd_req_t* req)
{
    set_cors(req);
    if (req->method == HTTP_OPTIONS) return send_response_options(req);
    if (req->method != HTTP_POST) return send_response_405(req);
    if (validate_content_type(req, "application/octet-stream") != ESP_OK) return ESP_OK;


    const server_ctx* ctx = req->user_ctx;

    const size_t expected_min = sizeof(uint16_t) + lb_led_matrix_width * lb_led_matrix_height * sizeof(lb_led_pixel);
    if (req->content_len < expected_min)
    {
        send_response_error(req, "400", "Invalid data length");
        return ESP_OK;
    }

    uint16_t name_len = 0;
    if (httpd_req_recv(req, (char*)&name_len, sizeof(name_len)) != sizeof(name_len))
    {
        send_response_error(req, "400", "Unable to read name length");
        return ESP_OK;
    }

    if (name_len > LB_MATRIX_NAME_MAX_LEN)
    {
        send_response_error(req, "400", "Name is too long");
        return ESP_OK;
    }

    if (req->content_len != expected_min + name_len)
    {
        send_response_error(req, "400", "Invalid data length");
        return ESP_OK;
    }

    char name[LB_MATRIX_NAME_MAX_LEN + 1] = {0};
    if (name_len > 0 && httpd_req_recv(req, name, name_len) != name_len)
    {
        send_response_error(req, "400", "Unable to read matrix name");
        return ESP_OK;
    }

    lb_led_matrix temp_matrix = {0};
    if (lb_led_matrix_read_binary(req, &temp_matrix) != ESP_OK)
    {
        send_response_error(req, "400", "Failed to read matrix");
        return ESP_OK;
    }

    static_assert(sizeof(name) == sizeof(temp_matrix.name), "Matrix name size mismatch");

    strncpy(temp_matrix.name, name, sizeof(name));

    xSemaphoreTake(ctx->x_gallery_write_lock, portMAX_DELAY);

    if (lb_gallery_add_image(&temp_matrix) != ESP_OK)
    {
        send_response_error(req, "500", "Unable to save matrix");
        return ESP_OK;
    }

    xSemaphoreGive(ctx->x_gallery_write_lock);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"success\": true}");
    return ESP_OK;
}


static esp_err_t delete_gallery(httpd_req_t* req)
{
    set_cors(req);
    if (req->method == HTTP_OPTIONS) return send_response_options(req);
    if (req->method != HTTP_DELETE) return send_response_405(req);


    char query_buf[HTTP_QUERY_MAX_LEN + 1];
    const size_t buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len <= 1)
    {
        const server_ctx* ctx = req->user_ctx;
        xSemaphoreTake(ctx->x_gallery_write_lock, portMAX_DELAY);

        const esp_err_t err = lb_gallery_delete_all();

        xSemaphoreGive(ctx->x_gallery_write_lock);

        if (err != ESP_OK)
        {
            send_response_error(req, "500", "Unable to clear gallery");
            return ESP_OK;
        }

        return httpd_resp_sendstr(req, "{\"success\": true}");
    }

    if (buf_len > HTTP_QUERY_MAX_LEN)
    {
        send_response_error(req, "400", "Query string is too big");
        return ESP_OK;
    }
    if (httpd_req_get_url_query_str(req, query_buf, buf_len) != ESP_OK)
    {
        send_response_error(req, "400", "Unable to read query string");
        return ESP_OK;
    }
    char param[HTTP_QUERY_VALUE_MAX_LEN + 1] = {0};
    esp_err_t err = httpd_query_key_value(query_buf, "index", param, sizeof(param) - 1);
    if (err != ESP_OK)
    {
        send_response_error(req, "400", err == ESP_ERR_NOT_FOUND
                                            ? "Query key index not found"
                                            : "Unable to parse query string");
        return ESP_OK;
    }

    const server_ctx* ctx = req->user_ctx;

    xSemaphoreTake(ctx->x_gallery_write_lock, portMAX_DELAY);

    const long index = strtol(param, NULL, 10);
    if (index < 0 || index >= lb_gallery_len())
    {
        xSemaphoreGive(ctx->x_gallery_write_lock);
        send_response_error(req, "400", "Invalid index");
        return ESP_OK;
    }

    err = lb_gallery_delete_image((int)index);

    xSemaphoreGive(ctx->x_gallery_write_lock);

    if (err != ESP_OK)
    {
        send_response_error(req, "500", "Unable to delete matrix");
        return ESP_OK;
    }

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, "{\"success\": true}");
}


static esp_err_t get_app_info(httpd_req_t* req)
{
    set_cors(req);
    if (req->method == HTTP_OPTIONS) return send_response_options(req);
    if (req->method != HTTP_GET) return send_response_405(req);

    const esp_app_desc_t* app = esp_app_get_description();
    char hash[sizeof(app->app_elf_sha256) * 2 + 1] = {0};
    for (int i = 0; i < sizeof(app->app_elf_sha256); ++i)
    {
        sprintf(&hash[i * 2], "%02x", app->app_elf_sha256[i]);
    }

    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "version", app->version);
    cJSON_AddStringToObject(root, "hashSHA256", hash);
    cJSON_AddNumberToObject(root, "magicWord", (double)app->magic_word);
    cJSON_AddStringToObject(root, "compileDate", app->date);
    cJSON_AddStringToObject(root, "compileTime", app->time);
    char* json_str = cJSON_Print(root);
    cJSON_Delete(root);
    if (json_str == NULL)
    {
        send_response_error(req, "500", "Unable to read app info");
        return ESP_OK;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_str);
    free(json_str);
    return ESP_OK;
}

static esp_err_t post_ota(httpd_req_t* req)
{
    set_cors(req);
    if (req->method == HTTP_OPTIONS) return send_response_options(req);
    if (req->method != HTTP_POST) return send_response_405(req);

    if (lb_ota_update(req) != ESP_OK) return ESP_OK; // OTA updater reports errors on its own

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, "{\"success\": true}");
}


lb_server_handle lb_start_webserver()
{
    server_ctx* ctx = malloc(sizeof(server_ctx));
    ctx->x_LED_write_lock = xSemaphoreCreateMutex();
    if (unlikely(ctx->x_LED_write_lock == NULL))
    {
        ESP_LOGE(TAG, "Failed to create LED read/write mutex!");
        abort();
    }
    ctx->x_gallery_write_lock = xSemaphoreCreateMutex();
    if (unlikely(ctx->x_gallery_write_lock == NULL))
    {
        ESP_LOGE(TAG, "Failed to create gallery read/write mutex!");
        abort();
    }


    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.max_uri_handlers = 12;
    config.max_resp_headers = 10;

    const httpd_uri_t uris[] = {
        {
            .uri = "/get_leds", .method = HTTP_ANY, .handler = get_leds_handler_bin, .user_ctx = ctx
        },
        {
            .uri = "/set_leds", .method = HTTP_ANY, .handler = set_leds_handler_bin, .user_ctx = ctx
        },
        {
            .uri = "/get_settings", .method = HTTP_ANY, .handler = get_settings, .user_ctx = ctx
        },
        {
            .uri = "/put_settings", .method = HTTP_ANY, .handler = put_settings, .user_ctx = ctx
        },
        {
            .uri = "/get_gallery", .method = HTTP_ANY, .handler = get_gallery, .user_ctx = ctx
        },
        {
            .uri = "/post_gallery", .method = HTTP_ANY, .handler = post_gallery, .user_ctx = ctx
        },
        {
            .uri = "/delete_gallery", .method = HTTP_ANY, .handler = delete_gallery, .user_ctx = ctx
        },
        {
            .uri = "/post_ota", .method = HTTP_ANY, .handler = post_ota, .user_ctx = ctx
        },
        {
            .uri = "/get_app_info", .method = HTTP_ANY, .handler = get_app_info, .user_ctx = ctx
        },
        {
            .uri = "/*", .method = HTTP_GET, .handler = get_front, .user_ctx = ctx
        },
    };

    if (httpd_start(&ctx->server, &config) == ESP_OK)
    {
        for (int i = 0; i < sizeof(uris) / sizeof(uris[0]); i++)
        {
            httpd_register_uri_handler(ctx->server, &uris[i]);
        }
    }

    return ctx;
}

void lb_stop_webserver(lb_server_handle handle)
{
    if (handle == NULL) return;

    const server_ctx* ctx = handle;

    if (ctx->server)
    {
        httpd_stop(ctx->server);
        vSemaphoreDelete(ctx->x_LED_write_lock);
    }
}
