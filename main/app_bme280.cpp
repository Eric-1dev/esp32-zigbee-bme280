#include "bme280.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "i2c_bus.h"
#include "esp_log.h"

#include "app_bme280.h"

#define I2C_SDA GPIO_NUM_1
#define I2C_SCL GPIO_NUM_2

static bme280_handle_t bme280 = NULL;

void bme280_init()
{
    i2c_config_t i2c_config = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_SDA,
        .scl_io_num = I2C_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master = {
            .clk_speed = 100000
        }
    };

    i2c_bus_handle_t i2c_bus = i2c_bus_create(I2C_NUM_0, &i2c_config);

    bme280 = bme280_create(i2c_bus, BME280_I2C_ADDRESS_DEFAULT);

    ESP_LOGI("BME280:", "bme280_default_init: %s", esp_err_to_name(bme280_default_init(bme280)));

    while (bme280_is_reading_calibration(bme280))
    {
        vTaskDelay(300 / portTICK_PERIOD_MS);
    }
}

float read_temperature() {
    float temp = 0;
    bme280_read_temperature(bme280, &temp);
    return temp;
}

float read_humidity() {
    float hum = 0;
    bme280_read_humidity(bme280, &hum);
    return hum;
}

float read_pressure() {
    float pres = 0;
    bme280_read_pressure(bme280, &pres);
    return pres;
}
