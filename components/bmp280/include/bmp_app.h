/*! \file bmp_app.h
    \brief bmp280 Sensor interface support header file */
#ifndef __BMPAPP_H__
#define __BMPAPP_H__

typedef struct
{
	float humidity;
	float pressure;
	float temperature;
}bmp_sensor_value_t;

void bmp280_mutex_init();
void bmp280_reader_task(void *ignore);
void bmp280_get_values(bmp_sensor_value_t * data);
float bmp280_get_humidity();
float bmp280_get_pressure();
float bmp280_get_temperature();

#endif