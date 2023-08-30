/*! \file wifi_app.h
    \brief Wifi interface c file */
#ifndef __WIFI_APP_H__
#define __WIFI_APP_H__

#include <esp_http_server.h>

void connect_wifi(void);
int wifi_connect_status_get(void);
httpd_handle_t setup_server(void);
void send_data_to_thingspeak(void *pvParameters);

#endif