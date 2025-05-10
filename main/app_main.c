#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_sleep.h"
#include "esp_log.h"
#include "app_bme280.h"
#include "app_zigbee.h"
#include "app_battery.h"

#define BUTTON_GPIO                 GPIO_NUM_9
#define SECONDS_TO_SLEEP            3           // Время сна
#define BUTTON_ACTIVITY_TIMEOUT_MS  5000       // Тайм-аут активности кнопки

static const char *TAG = "Sensor";

static volatile uint32_t last_button_activity_ms = 0; // Время последней активности кнопки

// Функция обработки прерывания по кнопке
static void IRAM_ATTR button_isr_handler(void *arg)
{
    last_button_activity_ms = esp_log_timestamp(); // Обновляем время активности
}

void button_init() {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = false,
        .pull_down_en = false,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    gpio_config(&io_conf);

    // Установка обработчика прерывания
    gpio_install_isr_service(0);
    gpio_isr_handler_add(BUTTON_GPIO, button_isr_handler, NULL);

    esp_sleep_enable_ext1_wakeup(1ULL << BUTTON_GPIO, ESP_EXT1_WAKEUP_ANY_LOW);
}

bool is_button_pressed_long(int timeout_seconds)
{
    int counter = 0;
    const int check_interval = 100; // ms

    while (gpio_get_level(BUTTON_GPIO) == 0)
    {
        vTaskDelay(pdMS_TO_TICKS(check_interval));
        counter += check_interval;
        last_button_activity_ms = esp_log_timestamp(); // Обновляем время активности
        if (counter >= timeout_seconds * 1000)
        {
            return true; // Долгое нажатие
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
        last_button_activity_ms = esp_log_timestamp(); // Считаем пробуждение активностью
        break;
    default:
        ESP_LOGI(TAG, "Первый запуск / сброс");
        break;
    }
}

// Проверка, можно ли засыпать
bool can_enter_deep_sleep()
{
    uint32_t current_time_ms = esp_log_timestamp();
    if (current_time_ms - last_button_activity_ms < BUTTON_ACTIVITY_TIMEOUT_MS)
    {
        ESP_LOGI(TAG, "Кнопка недавно была активна, откладываем сон");
        return false;
    }
    return true;
}

// Переход в глубокий сон
void enter_deep_sleep()
{
    while (!can_enter_deep_sleep())
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    esp_sleep_enable_timer_wakeup(SECONDS_TO_SLEEP * 1000000);
    ESP_LOGI(TAG, "Переход в глубокий сон...");
    esp_deep_sleep_start();
}

void send_data()
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

    vTaskDelay(pdMS_TO_TICKS(5000));
    enter_deep_sleep();
}

void app_main()
{
    if (is_button_pressed_long(5))
    {
        ESP_LOGI(TAG, "Обнаружено долгое нажатие (>5 сек). Сброс сети ZigBee...");
        factory_reset();
    }

    bme280_init();
    adc_init();
    button_init();
    check_wakeup_reason();

    register_zigbee_network_joined_callback(send_data);

    register_connection_failed_callback(enter_deep_sleep);

    xTaskCreate(zigbee_task, "zigbee", 8192, NULL, 5, NULL);
}