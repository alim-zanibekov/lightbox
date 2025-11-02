#ifndef LB_SERVER_H
#define LB_SERVER_H

typedef void* lb_server_handle;

lb_server_handle lb_start_webserver();

void lb_stop_webserver(lb_server_handle server);

#endif //SERVER_H
