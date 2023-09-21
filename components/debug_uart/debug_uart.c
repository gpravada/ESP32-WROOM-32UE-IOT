/* UART asynchronous example, that uses separate RX and TX tasks

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "debug_uart.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "string.h"
#include "driver/gpio.h"

QueueHandle_t queue;

static const int RX_BUF_SIZE = 1024;

#define UART UART_NUM_2

#if CONFIG_IDF_TARGET_ESP32C3
    #define TXD_PIN (GPIO_NUM_20)
    #define RXD_PIN (GPIO_NUM_21)
#else
    #define TXD_PIN (GPIO_NUM_17)
    #define RXD_PIN (GPIO_NUM_16)
#endif

static int debug_uart_send_data(const char* logName, const char* data);

void debug_uart_init(void)
{
    const uart_config_t uart_config =
    {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    // We won't use a buffer for sending data.
    uart_driver_install(UART, RX_BUF_SIZE * 2, 0, 0, NULL, 0);
    uart_param_config(UART, &uart_config);
    uart_set_pin(UART, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}

void debug_uart_tx_task(void *arg)
{
    int rx_buffer[50];
    static const char *TX_TASK_TAG = "TX_TASK";
    esp_log_level_set(TX_TASK_TAG, ESP_LOG_INFO);

    queue = xQueueCreate(5, sizeof(rx_buffer));
    if (queue == 0)
    {
        ESP_LOGI("UART_INIT", "Failed to create queue= %p\n", queue);
    }

    while (1)
    {
        if( xQueueReceive(queue, &(rx_buffer), (TickType_t)5))
        {
            debug_uart_send_data(TX_TASK_TAG, (const char*)rx_buffer);
            vTaskDelay(2000 / portTICK_PERIOD_MS);
        }
    }
}

void debug_uart_rx_task(void *arg)
{
    static const char *RX_TASK_TAG = "RX_TASK";
    esp_log_level_set(RX_TASK_TAG, ESP_LOG_INFO);
    uint8_t* data = (uint8_t*) malloc(RX_BUF_SIZE+1);

    while (1)
    {
        const int rxBytes = uart_read_bytes(UART, data, RX_BUF_SIZE, 1000 / portTICK_PERIOD_MS);
        if (rxBytes > 0)
        {
            data[rxBytes] = 0;
            xQueueSend(queue, (void*)data, (TickType_t)0);
            ESP_LOGI(RX_TASK_TAG, "Read %d bytes: '%s'", rxBytes, data);
            ESP_LOG_BUFFER_HEXDUMP(RX_TASK_TAG, data, rxBytes, ESP_LOG_INFO);
        }
    }

    free(data);
}

static int debug_uart_send_data(const char* logName, const char* data)
{
    const int len = strlen(data);
    const int txBytes = uart_write_bytes(UART_NUM_2, data, len);
    ESP_LOGI(logName, "Wrote %d bytes: '%s'", txBytes, data);
    return txBytes;
}
