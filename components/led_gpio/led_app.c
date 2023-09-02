/*
****************************************************************************
* Copyright (C) 2023 - QualiZeal
*
* led_app.c
* Date: 2016/07/04
* Revision: 2.0.5(Pressure and Temperature compensation code revision is 1.1
*               and Humidity compensation code revision is 1.0)
*
* Usage: LED over GPIO.
*
****************************************************************************/

#include "led_app.h"

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#define LED_GPIO_PIN 5

void led_init()
{
    esp_rom_gpio_pad_select_gpio(LED_GPIO_PIN);
    gpio_set_direction(LED_GPIO_PIN, GPIO_MODE_OUTPUT);
}

void led_blink_task(void *ignore)
{
    int led_state = 1u;

    while(true)
    {
        led_state = !led_state;
        gpio_set_level(LED_GPIO_PIN, led_state);
        vTaskDelay(1000/portTICK_PERIOD_MS);
    }
}
