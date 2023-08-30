#include "wifi_app.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h> //Requires by memset
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"

#include "esp_wifi.h"
#include "esp_event.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include <lwip/sockets.h>
#include <lwip/sys.h>
#include <lwip/api.h>
#include <lwip/netdb.h>

#include "bmp_app.h"

#include "esp_http_client.h"

static char api_key[] = "0AA7W30FXAY7V1OW"; // my write api key
//char api_key[] = "H6IFDVO5YPJLPMP8"; // my read api key

static char thingspeak_url[] = "https://api.thingspeak.com";
static char data[] = "/update?api_key=%s&field1=%.2f&field2=%.2f&field3=%.2f";
static char post_data[200];
static esp_err_t err;

static esp_http_client_config_t config = {
    .url = thingspeak_url,
    .method = HTTP_METHOD_GET,
};

#define TAG_WIFI "WIFI-APP"

#define ESP_WIFI_SSID       "Yoshitha" // "QualiZeal"
#define ESP_WIFI_PASSWORD   "Pastword@2021" // "Q@Zeal0802"
#define ESP_MAXIMUM_RETRY 5

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

static int s_retry_num = 0;
static int wifi_connect_status = 0;

static double temperature  = 0.0f;
static double pressure = 0.0f;
static double humidity   = 0.0f;

static esp_err_t get_req_handler(httpd_req_t *req);
static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data);
static esp_err_t send_web_page(httpd_req_t *req);
static esp_err_t get_req_handler(httpd_req_t *req);

static httpd_uri_t uri_get = {
                              .uri = "/",
                              .method = HTTP_GET,
                              .handler = get_req_handler,
                              .user_ctx = NULL};

static char html_page[] =   "<!DOCTYPE HTML><html>\n"
                            "<head>\n"
                            "  <title>ESP-IDF BMP280 Web Server</title>\n"
                            "  <meta http-equiv=\"refresh\" content=\"5\">\n"
                            "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
                            "  <link rel=\"stylesheet\" href=\"https://use.fontawesome.com/releases/v5.7.2/css/all.css\" integrity=\"sha384-fnmOCqbTlWIlj8LyTjo7mOUStjsKC4pOpQbqyi7RrhN7udi9RwhKkMHpvLbHG9Sr\" crossorigin=\"anonymous\">\n"
                            "  <link rel=\"icon\" href=\"data:,\">\n"
                            "  <style>\n"
                            "    html {font-family: Arial; display: inline-block; text-align: center;}\n"
                            "    p {  font-size: 1.2rem;}\n"
                            "    body {  margin: 0;}\n"
                            "    .topnav { overflow: hidden; background-color: #4B1D3F; color: white; font-size: 1.7rem; }\n"
                            "    .content { padding: 20px; }\n"
                            "    .card { background-color: white; box-shadow: 2px 2px 12px 1px rgba(140,140,140,.5); }\n"
                            "    .cards { max-width: 700px; margin: 0 auto; display: grid; grid-gap: 2rem; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr)); }\n"
                            "    .reading { font-size: 2.8rem; }\n"
                            "    .card.temperature { color: #0e7c7b; }\n"
                            "    .card.humidity { color: #17bebb; }\n"
                            "    .card.pressure { color: #3fca6b; }\n"
                            "    .card.gas { color: #d62246; }\n"
                            "  </style>\n"
                            "</head>\n"
                            "<body>\n"
                            "  <div class=\"topnav\">\n"
                            "    <h3>ESP-IDF BMP280 WEB SERVER</h3>\n"
                            "  </div>\n"
                            "  <div class=\"content\">\n"
                            "    <div class=\"cards\">\n"
                            "      <div class=\"card temperature\">\n"
                            "        <h4><i class=\"fas fa-thermometer-half\"></i> TEMPERATURE</h4><p><span class=\"reading\">%.2f&deg;C</span></p>\n"
                            "      </div>\n"
                            "      <div class=\"card humidity\">\n"
                            "        <h4><i class=\"fas fa-tint\"></i> HUMIDITY</h4><p><span class=\"reading\">%.2f</span> &percnt;</span></p>\n"
                            "      </div>\n"
                            "      <div class=\"card pressure\">\n"
                            "        <h4><i class=\"fas fa-angle-double-down\"></i> PRESSURE</h4><p><span class=\"reading\">%.2fhPa</span></p>\n"
                            "      </div>\n"
                            "    </div>\n"
                            "  </div>\n"
                            "</body>\n"
                            "</html>";

static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        if (s_retry_num < ESP_MAXIMUM_RETRY)
        {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI("MAIN-APP", "retry to connect to the AP");
        }
        else
        {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        wifi_connect_status = 0;
        ESP_LOGI("MAIN-APP", "connect to the AP fail");
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI("MAIN-APP", "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        wifi_connect_status = 1;
    }
}

static esp_err_t send_web_page(httpd_req_t *req)
{
    int response;

    //Get readings from BMP280 sensor.
    temperature = bmp280_get_temperature();
    pressure = bmp280_get_pressure();
    humidity = bmp280_get_humidity();

    char response_data[sizeof(html_page) + 50];
    memset(response_data, 0, sizeof(response_data));
    sprintf(response_data, html_page, temperature, humidity, pressure);
    response = httpd_resp_send(req, response_data, HTTPD_RESP_USE_STRLEN);

    return response;
}

static esp_err_t get_req_handler(httpd_req_t *req)
{
    return send_web_page(req);
}

void connect_wifi(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *netif = esp_netif_create_default_wifi_sta();

    // Set the hostname for the network interface
    esp_netif_set_hostname(netif, "ESP32_IOT_SENSOR");

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = ESP_WIFI_SSID,
            .password = ESP_WIFI_PASSWORD,
            /* Setting a password implies station will connect to all security modes including WEP/WPA.
             * However these modes are deprecated and not advisable to be used. Incase your Access point
             * doesn't support WPA2, these mode can be enabled by commenting below line */
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI("MAIN-APP", "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT)
    {
        ESP_LOGI("MAIN-APP", "connected to ap SSID:%s", ESP_WIFI_SSID);
    }
    else if (bits & WIFI_FAIL_BIT)
    {
        ESP_LOGI("MAIN-APP", "Failed to connect to SSID:%s", ESP_WIFI_SSID);
    }
    else
    {
        ESP_LOGE("MAIN-APP", "UNEXPECTED EVENT");
    }

    /* The event will not be processed after unregister */
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));

    vEventGroupDelete(s_wifi_event_group);
}

int wifi_connect_status_get(void)
{
    return wifi_connect_status;
}

httpd_handle_t setup_server(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;

    if (httpd_start(&server, &config) == ESP_OK)
    {
        httpd_register_uri_handler(server, &uri_get);
    }

    return server;
}

void send_data_to_thingspeak(void *pvParameters)
{
	esp_http_client_handle_t client = esp_http_client_init(&config);
	esp_http_client_set_header(client, "Content-Type", "application/x-www-form-urlencoded");
	while (1)
	{
		vTaskDelay(5000 / portTICK_PERIOD_MS);

        //Get readings from BMP280 sensor.
        temperature = bmp280_get_temperature();
        pressure = bmp280_get_pressure();
        humidity = bmp280_get_humidity();

		strcpy(post_data, "");
		snprintf(post_data, sizeof(post_data), data, api_key, temperature, pressure, humidity);
		ESP_LOGI("HTTP_CLIENT", "post = %s", post_data);
		esp_http_client_set_post_field(client, post_data, strlen(post_data));

		err = esp_http_client_perform(client);

		if (err == ESP_OK)
		{
			int status_code = esp_http_client_get_status_code(client);
			if (status_code == 200)
			{
				ESP_LOGI("HTTP_CLIENT", "Message sent Successfully");
			}
			else
			{
				ESP_LOGI("HTTP_CLIENT", "Message sent Failed");
				goto exit;
			}
		}
		else
		{
			ESP_LOGI("HTTP_CLIENT", "Message sent Failed");
			goto exit;
		}
	}
exit:
	esp_http_client_cleanup(client);
	vTaskDelete(NULL);
}