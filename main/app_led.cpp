#include <stdio.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "app_led.h"

static bool led_on = false;
static bool cur_state = false;
static uint16_t led_period_ms = 0;

void led_turn_on(uint16_t period_ms)
{
    led_on = true;
    led_period_ms = period_ms;
}

void led_turn_off()
{
    led_on = false;
}

void led_init()
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << LED_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
}

void led_task(void *pvParameters)
{
    led_init();

    while (1)
    {
        if (!led_on)
        {
            gpio_set_level(LED_GPIO, 0);
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        gpio_set_level(LED_GPIO, 1);
        vTaskDelay(pdMS_TO_TICKS(30));
        gpio_set_level(LED_GPIO, 0);
        vTaskDelay(pdMS_TO_TICKS(led_period_ms));
    }
}