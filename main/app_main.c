#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_sleep.h"
#include "esp_log.h"
#include "esp_pm.h"
#include "app_bme280.h"
#include "app_zigbee.h"

// Пины
#define BAT_ADC_CHAN ADC_CHANNEL_3 // GPIO4
#define BUTTON_GPIO  GPIO_NUM_9

static const char *TAG = "Sensor";

static adc_oneshot_unit_handle_t adc1_handle;
static adc_cali_handle_t adc1_cali_handle = NULL;

void battery_init() {
    // Настройка АЦП
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
        .clk_src = ADC_DIGI_CLK_SRC_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

    // Конфигурация канала
    adc_oneshot_chan_cfg_t chan_config = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, BAT_ADC_CHAN, &chan_config));

    // Калибровка
    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = ADC_UNIT_1,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    if (adc_cali_create_scheme_curve_fitting(&cali_config, &adc1_cali_handle) != ESP_OK) {
        ESP_LOGE(TAG, "Ошибка калибровки АЦП");
    }
}

float read_battery_voltage() {
    int adc_raw;
    int voltage_mv;

    ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, BAT_ADC_CHAN, &adc_raw));
    if (adc1_cali_handle) {
        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_handle, adc_raw, &voltage_mv));
    } else {
        voltage_mv = adc_raw;
    }

    float voltage_v = voltage_mv * 2 / 1000.0f; // Учтём делитель 1:2
    return voltage_v;
}

void button_init() {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = true,
        .pull_down_en = false,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    gpio_config(&io_conf);
    esp_sleep_enable_gpio_wakeup();
}

void check_wakeup_reason() {
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    switch (cause) {
        case ESP_SLEEP_WAKEUP_TIMER:
            ESP_LOGI(TAG, "Пробуждение по таймеру");
            break;
        case ESP_SLEEP_WAKEUP_GPIO:
            ESP_LOGI(TAG, "Пробуждение по кнопке");
            break;
        default:
            ESP_LOGI(TAG, "Первый запуск / сброс");
            break;
    }
}

void enter_deep_sleep(int seconds) {
    esp_sleep_enable_timer_wakeup(seconds * 1000000);
    button_init();
    ESP_LOGI(TAG, "Переход в глубокий сон...");
    esp_deep_sleep_start();
}

uint8_t calc_battery_percent(float voltage)
{
    int16_t value = (voltage - 3.3) * 100 / (4.2 - 3.3);
    if (value < 0)
        value = 0;

    if (value > 100)
        value = 100;

    return (uint8_t)value;
}

void sensor_loop() {
    while (1) {
        float temp = read_temperature();
        float hum = read_humidity();
        float press = read_pressure();
        float bat = read_battery_voltage();

        uint8_t mapped_battery = calc_battery_percent(bat);

        update_temperature_value((int16_t)(temp * 100));
        update_humidity_value((uint16_t)(hum * 100));
        update_pressure_value((int16_t)(press * 10));
        update_battery_value(mapped_battery);

        ESP_LOGI(TAG, "Температура: %.2f °C", temp);
        ESP_LOGI(TAG, "Влажность: %.2f %%", hum);
        ESP_LOGI(TAG, "Давление: %.2f hPa", press);
        ESP_LOGI(TAG, "Напряжение: %.2f V", bat);

        vTaskDelay(pdMS_TO_TICKS(3000));
        //enter_deep_sleep(3);
    }
}

void app_main() {
    bme280_init();
    battery_init();
    check_wakeup_reason();

    xTaskCreate(zigbee_task, "zigbee", 8192, NULL, 5, NULL);
    vTaskDelay(pdMS_TO_TICKS(5000)); // дать ZigBee стартовать

    sensor_loop();
}