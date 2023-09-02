#include "ble_app.h"
#include "bmp280.h"
#include "bmp_app.h"
#include "led_app.h"
#include "debug_uart.h"
#include "nvs_flash.h"
#include "wifi_app.h"

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "freertos/task.h"

// Main pplication start.
void app_main(void)
{
    /* Initialize NVS — it is used to store PHY calibration data */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // LED init.
    led_init();
    xTaskCreate(&led_blink_task, "led_blink_task",  1024, NULL, 6, NULL);

    // bmp280 initialization.
    bmp280_mutex_init();
    ESP_ERROR_CHECK(i2cdev_init());
    xTaskCreatePinnedToCore(bmp280_reader_task, "bmp280_reader_task", configMINIMAL_STACK_SIZE * 8, NULL, 5, NULL, APP_CPU_NUM);

    // Nimble BLE intialization
    ble_init();

    ESP_LOGI("MAIN-APP", "ESP_WIFI_MODE_STA\n");
    connect_wifi();

    if (wifi_connect_status_get())
    {
		//xTaskCreate(&send_data_to_thingspeak, "send_data_to_thingspeak", 8192, NULL, 6, NULL);
        setup_server();
        ESP_LOGI("MAIN-APP", "BMP280 Web Server is up and running\n");
    }
    else
        ESP_LOGI("MAIN-APP", "Failed to connected with Wi-Fi, check your network Credentials\n");

    //UART task
#ifdef DEBUG_UART_TEST
    debug_uart_init();
    xTaskCreate(debug_uart_rx_task, "uart_rx_task", 1024*2, NULL, configMAX_PRIORITIES, NULL);
    xTaskCreate(debug_uart_tx_task, "uart_tx_task", 1024*2, NULL, configMAX_PRIORITIES-1, NULL);
#endif
}
