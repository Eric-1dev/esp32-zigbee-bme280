#ifndef APP_ZIGBEE_H
#define APP_ZIGBEE_H

#include <stdio.h>

#define MANUFACTURER_NAME           "\x08""Eric Inc"
#define MODEL_IDENTIFIER            "\x07"CONFIG_IDF_TARGET

#define INSTALLCODE_POLICY_ENABLE   false
#define ED_AGING_TIMEOUT            ESP_ZB_ED_AGING_TIMEOUT_64MIN
#define ED_KEEP_ALIVE               1000 /* 1000 millisecond */
#define HA_ESP_SENSOR_ENDPOINT      10 /* esp temperature sensor device endpoint, used for temperature measurement */

typedef void (*zigbee_network_event_cb)(void);

void zigbee_task(void *pvParameters);
void factory_reset(void);
void update_temperature_value(int16_t temperature_degrees_tenths);
void update_humidity_value(uint16_t humidity_tenths);
void update_pressure_value(int16_t pressure_tenths);
void update_battery_value(uint8_t battery_percent);
void register_zigbee_network_joined_callback(zigbee_network_event_cb cb);
void register_connection_failed_callback(zigbee_network_event_cb cb);

#endif