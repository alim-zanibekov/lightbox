#ifndef LB_OTA_H
#define LB_OTA_H
#include "esp_http_server.h"

void lb_ota_init();

esp_err_t lb_ota_update(httpd_req_t* req);

#endif //LB_OTA_H
