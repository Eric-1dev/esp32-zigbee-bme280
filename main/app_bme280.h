#ifndef BME280_H
#define BME280_H

void bme280_init();
float read_temperature();
float read_humidity();
float read_pressure();

#endif