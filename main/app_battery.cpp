#include <stdio.h>
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"
#include "app_battery.h"

static const char *TAG = "Battery";

static adc_oneshot_unit_handle_t adc1_handle;
static adc_cali_handle_t adc1_cali_handle = NULL;

void adc_init()
{
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
    if (adc_cali_create_scheme_curve_fitting(&cali_config, &adc1_cali_handle) != ESP_OK)
    {
        ESP_LOGE(TAG, "Ошибка калибровки АЦП");
    }
}

float read_battery_voltage()
{
    int adc_raw;
    int voltage_mv;

    ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, BAT_ADC_CHAN, &adc_raw));
    if (adc1_cali_handle)
    {
        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_handle, adc_raw, &voltage_mv));
    }
    else
    {
        voltage_mv = adc_raw;
    }

    float voltage_v = voltage_mv * 2 / 1000.0f; // Делитель 1:2
    return voltage_v;
}

uint8_t calc_battery_percent(float voltage)
{
    int16_t value = (voltage - 3.0) * 100 / (4.2 - 3.0);
    if (value < 0)
        value = 0;

    if (value > 100)
        value = 100;

    return (uint8_t)value;
}