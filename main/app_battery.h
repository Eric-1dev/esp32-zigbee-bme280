#include <stdio.h>

#define BAT_ADC_CHAN ADC_CHANNEL_3 // GPIO4

void adc_init();
float read_battery_voltage();
uint8_t calc_battery_percent(float voltage);