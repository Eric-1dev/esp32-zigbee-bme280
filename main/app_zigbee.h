#ifndef APP_ZIGBEE_H
#define APP_ZIGBEE_H

#include <stdio.h>

#define MANUFACTURER_NAME           "\x08""Eric Inc"
#define MODEL_IDENTIFIER            "\x0C""SensorBME280"

#define INSTALLCODE_POLICY_ENABLE   false
#define ED_AGING_TIMEOUT            ESP_ZB_ED_AGING_TIMEOUT_64MIN
#define ED_KEEP_ALIVE               3000 /* 1000 millisecond */
#define HA_ESP_SENSOR_ENDPOINT      10 /* esp temperature sensor device endpoint, used for temperature measurement */

typedef void (*zigbee_network_event_cb)(void);

void zigbee_task(void *pvParameters);
void factory_reset(void);
void update_temperature_value(int16_t temperature_degrees_tenths);
void update_humidity_value(uint16_t humidity_tenths);
void update_pressure_value(int16_t pressure_tenths);
void update_battery_value(uint8_t battery_percent);
void send_device_announce(void);
void register_factory_reset_mode_callback(zigbee_network_event_cb cb);
void register_network_joined_callback(zigbee_network_event_cb cb);
void register_connection_failed_callback(zigbee_network_event_cb cb);
void register_reboot_success_callback(zigbee_network_event_cb cb);

#endif