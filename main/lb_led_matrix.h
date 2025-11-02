#ifndef LB_LED_MATRIX_H
#define LB_LED_MATRIX_H

#include <stdint.h>

#include "esp_err.h"

#define LB_MATRIX_NAME_MAX_LEN 24
#define LB_MATRIX_COLUMNS 13
#define LB_MATRIX_ROWS 23
#define LB_MATRIX_LED_NUM 300

typedef struct
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
} lb_led_pixel;

typedef struct
{
    char name[LB_MATRIX_NAME_MAX_LEN + 1];
    lb_led_pixel m[LB_MATRIX_ROWS][LB_MATRIX_COLUMNS];
} lb_led_matrix;

typedef struct
{
    uint8_t brightness;
    lb_led_matrix matrix;
} lb_led_matrix_state;

extern const int lb_led_matrix_width;
extern const int lb_led_matrix_height;

void lb_led_matrix_init();

void lb_led_matrix_destroy();

uint8_t lb_led_matrix_get_brightness();

void lb_led_matrix_set_brightness(uint8_t brightness);

void lb_led_matrix_set_pixel_buffer(int i, int j, uint8_t r, uint8_t g, uint8_t b);

const lb_led_matrix* lb_led_matrix_get_matrix();

void lb_led_matrix_set_matrix(const lb_led_matrix* matrix);

lb_led_pixel* lb_led_matrix_get_pixel_buffer(int i, int j);

void lb_led_matrix_apply_buffer();

esp_err_t lb_led_matrix_save(const lb_led_matrix* matrix, const char* path);

esp_err_t lb_led_matrix_load(lb_led_matrix* matrix, const char* path);

void lb_led_matrix_save_state();

void lb_led_matrix_load_state();

#endif //LB_LED_MATRIX_H
