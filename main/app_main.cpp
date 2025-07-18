#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_sleep.h"
#include "esp_log.h"
#include "app_bme280.h"
#include "app_zigbee.h"
#include "app_battery.h"
#include "app_button.h"
#include "app_led.h"

#define SECONDS_TO_SLEEP 120               // Время сна

static const char *TAG = "Sensor";

// Проверка причины пробуждения
void check_wakeup_reason(void)
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

// Переход в глубокий сон
void enter_deep_sleep(void)
{
    esp_sleep_enable_timer_wakeup(SECONDS_TO_SLEEP * 1000000);
    ESP_LOGI(TAG, "Переход в глубокий сон...");
    esp_deep_sleep_start();
}

void send_data(void)
{
    float temp = read_temperature();
    float hum = read_humidity();
    float press = read_pressure() / 10;
    float bat = read_battery_voltage();

    uint8_t battery_percent = calc_battery_percent(bat);

    update_temperature_value((int16_t)(temp * 100));
    update_humidity_value((uint16_t)(hum * 100));
    update_pressure_value((int16_t)(press * 10));
    update_battery_value(battery_percent);

    ESP_LOGI(TAG, "Температура: %.2f °C", temp);
    ESP_LOGI(TAG, "Влажность: %.2f %%", hum);
    ESP_LOGI(TAG, "Давление: %.2f kPa", press);
    ESP_LOGI(TAG, "Напряжение: %.2f V", bat);
}

void send_data_once()
{
    send_data();
    vTaskDelay(pdMS_TO_TICKS(500));
    enter_deep_sleep();
}

void send_data_periodically()
{
    for (uint8_t i = 0; i < 3; i++)
    {
        vTaskDelay(pdMS_TO_TICKS(10000));
        send_data();
    }

    vTaskDelay(pdMS_TO_TICKS(500));
    enter_deep_sleep();
}

void zigbee_event_handler_task(void *pvParameters)
{
    zigbee_event_t event;

    while (1)
    {
        if (!xQueueReceive(zigbee_event_queue, &event, portMAX_DELAY))
        {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        switch (event.type)
        {
            case ZB_EVENT_REBOOT_SUCCESS:
                send_data_once();
                break;
            case ZB_EVENT_FACTORY_RESET_MODE:
                led_turn_on(200);
                break;
            case ZB_EVENT_NETWORK_JOINED:
                led_turn_on(500);
                send_data_periodically();
                break;
            case ZB_EVENT_CONNECTION_FAILED:
                enter_deep_sleep();
                break;
        }
    }
}

#ifdef __cplusplus
extern "C"
{
#endif

    void app_main(void)
    {
        bme280_init();
        adc_init();
        check_wakeup_reason();

        // Создаем очередь для событий Zigbee
        zigbee_event_queue = xQueueCreate(10, sizeof(zigbee_event_t));

        // Регистрируем обработчики кнопок
        register_short_press_callback([]()
        {
            send_data();
        });

        register_long_press_callback([]()
        {
            ESP_LOGI(TAG, "Обнаружено долгое нажатие. Сброс сети ZigBee...");
            led_turn_on(200);
            vTaskDelay(pdMS_TO_TICKS(2000));
            led_turn_off();
            factory_reset();
        });

        xTaskCreate(button_task, "button_task", 4096, NULL, 6, NULL);
        xTaskCreate(led_task, "led_task", 2048, NULL, 6, NULL);
        xTaskCreate(zigbee_event_handler_task, "zigbee_event_handler", 4096, NULL, 5, NULL);
        xTaskCreate(zigbee_task, "zigbee", 8192, NULL, 5, NULL);
    }

#ifdef __cplusplus
}
#endif