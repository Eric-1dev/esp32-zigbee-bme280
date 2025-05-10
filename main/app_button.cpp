#include <stdio.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "app_button.h"

static const char *TAG = "Button";

static button_event_cb long_press_callback = []() {};
static button_event_cb short_press_callback = []() {};

void register_long_press_callback(button_event_cb cb)
{
    long_press_callback = cb;
}

void register_short_press_callback(button_event_cb cb)
{
    short_press_callback = cb;
}

bool is_button_pressed()
{
    return gpio_get_level(BUTTON_GPIO) == 0;
}

// Инициализация кнопки
void button_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    gpio_config(&io_conf);

    // Настройка пробуждения из глубокого сна
    esp_sleep_enable_ext1_wakeup(1ULL << BUTTON_GPIO, ESP_EXT1_WAKEUP_ANY_LOW);
}

// Задача для обработки кнопки
void button_task(void *pvParameters)
{
    const int check_interval_ms = 100;      // Интервал проверки состояния кнопки

    button_init();

    while (1)
    {
        if (is_button_pressed()) // Кнопка нажата
        {
            int counter = 0;
            bool is_long_press = false;
            bool is_short_press = true;

            // Проверяем, как долго кнопка удерживается
            while (is_button_pressed())
            {
                vTaskDelay(pdMS_TO_TICKS(check_interval_ms));
                counter += check_interval_ms;

                if (counter >= SHORT_PRESS_TIEMOUT_MS)
                {
                    is_short_press = false;
                }

                if (counter >= LONG_PRESS_TIEMOUT_MS)
                {
                    is_long_press = true;
                    break;
                }
            }

            if (is_long_press)
            {
                is_long_press = false;
                long_press_callback();
            }
            else if (is_short_press)
            {
                is_short_press = false;
                short_press_callback();
            }
        }

        vTaskDelay(pdMS_TO_TICKS(50)); // Периодическая проверка кнопки
    }
}