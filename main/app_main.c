#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_sleep.h"
#include "esp_log.h"
#include "app_bme280.h"
#include "app_zigbee.h"
#include "app_battery.h"

#define BUTTON_GPIO  GPIO_NUM_9
#define MAX_CONNECTION_ATTEMPTS 10
#define ATTEMPT_INTERVAL_MS 1000

static const char *TAG = "Sensor";

void button_init() {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = false,
        .pull_down_en = false,
        .intr_type = GPIO_INTR_LOW_LEVEL,
    };
    gpio_config(&io_conf);
    esp_sleep_enable_ext1_wakeup(1ULL << BUTTON_GPIO, ESP_EXT1_WAKEUP_ANY_LOW);
}

bool is_button_pressed_long(int timeout_seconds)
{
    int counter = 0;
    const int check_interval = 100; // ms

    while (gpio_get_level(BUTTON_GPIO) == 0) // пока кнопка нажата
    {
        vTaskDelay(pdMS_TO_TICKS(check_interval));
        counter += check_interval;

        if (counter >= timeout_seconds * 1000)
        {
            return true; // долгое нажатие
        }
    }

    return false;
}

void check_wakeup_reason()
{
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    switch (cause)
    {
    case ESP_SLEEP_WAKEUP_TIMER:
        ESP_LOGI(TAG, "Пробуждение по таймеру");
        break;
    case ESP_SLEEP_WAKEUP_EXT1:
        ESP_LOGI(TAG, "Пробуждение по кнопке");
        break;
    default:
        ESP_LOGI(TAG, "Первый запуск / сброс");
        break;
    }
}

void enter_deep_sleep(int seconds) {
    esp_sleep_enable_timer_wakeup(seconds * 1000000);
    ESP_LOGI(TAG, "Переход в глубокий сон...");
    esp_deep_sleep_start();
}

bool wait_for_network()
{
    for (int i = 0; i < MAX_CONNECTION_ATTEMPTS; i++)
    {
        if (is_network_joined())
        {
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(ATTEMPT_INTERVAL_MS));
    }
    return false;
}

void sensor_loop() {
    while (1) {

        if (!wait_for_network())
        {
            ESP_LOGE(TAG, "Не удалось подключиться к сети");
        }
        else 
        {
            float temp = read_temperature();
            float hum = read_humidity();
            float press = read_pressure();
            float bat = read_battery_voltage();

            uint8_t battery_percent = calc_battery_percent(bat);

            update_temperature_value((int16_t)(temp * 100));
            update_humidity_value((uint16_t)(hum * 100));
            update_pressure_value((int16_t)(press * 10));
            update_battery_value(battery_percent);

            ESP_LOGI(TAG, "Температура: %.2f °C", temp);
            ESP_LOGI(TAG, "Влажность: %.2f %%", hum);
            ESP_LOGI(TAG, "Давление: %.2f hPa", press);
            ESP_LOGI(TAG, "Напряжение: %.2f V", bat);
        }

        vTaskDelay(pdMS_TO_TICKS(2000));
        enter_deep_sleep(30);
    }
}

void app_main() {
    if (is_button_pressed_long(5))
    {
        ESP_LOGI(TAG, "Обнаружено долгое нажатие (>5 сек). Сброс сети ZigBee...");
        factory_reset();
    }

    bme280_init();
    adc_init();
    button_init();
    check_wakeup_reason();

    xTaskCreate(zigbee_task, "zigbee", 8192, NULL, 5, NULL);

    wait_for_network();

    sensor_loop();
}