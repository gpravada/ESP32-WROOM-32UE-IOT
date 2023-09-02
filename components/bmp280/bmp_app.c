/*
****************************************************************************
* Copyright (C) 2023 - QualiZeal
*
* bmp_app.c
* Date: 2016/07/04
* Revision: 2.0.5(Pressure and Temperature compensation code revision is 1.1
*               and Humidity compensation code revision is 1.0)
*
* Usage: Sensor Driver interface file for BMP280 sensor
*
****************************************************************************/

#include "bmp_app.h"
#include "bmp280.h"

#include "driver/i2c.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/task.h"

#include <string.h>

#define SDA_PIN GPIO_NUM_22
#define SCL_PIN GPIO_NUM_21

#define TAG_BMP280 "BMP280"

#define I2C_MASTER_ACK 0
#define I2C_MASTER_NACK 1

#ifndef APP_CPU_NUM
#define APP_CPU_NUM PRO_CPU_NUM
#endif

SemaphoreHandle_t xMutex;

static bmp_sensor_value_t bmp280_data;

void bmp280_mutex_init()
{
	xMutex = xSemaphoreCreateMutex();
}

void bmp280_reader_task(void *ignore)
{
    bmp280_params_t params;
    bmp280_init_default_params(&params);
    bmp280_t dev;
    memset(&dev, 0, sizeof(bmp280_t));

    ESP_ERROR_CHECK(bmp280_init_desc(&dev, BMP280_I2C_ADDRESS_0, 0, GPIO_NUM_22, GPIO_NUM_21));
    ESP_ERROR_CHECK(bmp280_init(&dev, &params));

    bool bme280p = dev.id == BME280_CHIP_ID;
    printf("BMP280: found %s\n", bme280p ? "BME280" : "BMP280");

    float pressure, temperature, humidity;

    while (!bme280p)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
        if (bmp280_read_float(&dev, &temperature, &pressure, &humidity) != ESP_OK)
        {
            printf("Temperature/pressure reading failed\n");
            continue;
        }

        if (xSemaphoreTake(xMutex, portMAX_DELAY))
        {
			bmp280_data.temperature = temperature;
			bmp280_data.pressure 	= pressure/100;
			bmp280_data.humidity 	= 0.0f;

        	//printf("Pressure: %.2f hPa, Temperature: %.2f degC", pressure/100, temperature);
			xSemaphoreGive(xMutex);
		}
    }

	vTaskDelete(NULL);
}

void bmp280_get_values(bmp_sensor_value_t * bmp_sensor_data)
{
	if (xSemaphoreTake(xMutex, portMAX_DELAY))
	{
		bmp_sensor_data->humidity 	  = bmp280_data.humidity;
		bmp_sensor_data->pressure 	  = bmp280_data.pressure;
		bmp_sensor_data->temperature  = bmp280_data.temperature;
		xSemaphoreGive(xMutex);
	}
}

float bmp280_get_humidity()
{
	float humidity = 0.0;
	if (xSemaphoreTake(xMutex, portMAX_DELAY))
	{
		humidity = bmp280_data.humidity;
		xSemaphoreGive(xMutex);
	}
	return humidity;
}

float bmp280_get_pressure()
{
	float pressure = 0.0;
	if (xSemaphoreTake(xMutex, portMAX_DELAY))
	{
		pressure = bmp280_data.pressure;
		xSemaphoreGive(xMutex);
	}
	return pressure;
}

float bmp280_get_temperature()
{
	float temperature = 0.0;
	if (xSemaphoreTake(xMutex, portMAX_DELAY))
	{
		temperature = bmp280_data.temperature;
		xSemaphoreGive(xMutex);
	}
	return temperature;
}

