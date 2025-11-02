#ifndef LB_GALLERY_H
#define LB_GALLERY_H

#include "lb_led_matrix.h"

#define LB_GALLERY_CAPACITY 20

extern const int lb_gallery_capacity;

void lb_gallery_init();

esp_err_t lb_gallery_add_image(const lb_led_matrix* matrix);

esp_err_t lb_gallery_delete_image(const int i);

esp_err_t lb_gallery_delete_all();

esp_err_t lb_gallery_read(const int i, lb_led_matrix* matrix);

size_t lb_gallery_len();

#endif //LB_GALLERY_H
