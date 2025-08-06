#ifndef APP_ZIGBEE_H
#define APP_ZIGBEE_H

#include <stdio.h>

#define MANUFACTURER_NAME           "\x08""Eric Inc"
#define MODEL_IDENTIFIER            "\x0C""SensorBME280"

#define INSTALLCODE_POLICY_ENABLE   false
#define ED_AGING_TIMEOUT            ESP_ZB_ED_AGING_TIMEOUT_64MIN
#define ED_KEEP_ALIVE               3000 /* 1000 millisecond */
#define HA_ESP_SENSOR_ENDPOINT      10 /* esp temperature sensor device endpoint, used for temperature measurement */

typedef enum
{
    ZB_EVENT_REBOOT_SUCCESS,
    ZB_EVENT_FACTORY_RESET_MODE,
    ZB_EVENT_NETWORK_JOINED,
    ZB_EVENT_CONNECTION_FAILED
} zigbee_event_type_t;

typedef struct
{
    zigbee_event_type_t type;
} zigbee_event_t;

extern QueueHandle_t zigbee_event_queue;

void zigbee_task(void *pvParameters);
void factory_reset(void);
void update_temperature_value(int16_t temperature_degrees_tenths);
void update_humidity_value(uint16_t humidity_tenths);
void update_pressure_value(int16_t pressure_tenths);
void update_battery_percent_value(uint8_t battery_percent);

#endif