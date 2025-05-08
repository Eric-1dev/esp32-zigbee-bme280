#include <stdio.h>
#include "app_zigbee.h"
#include "esp_zigbee_core.h"
#include "zcl/esp_zigbee_zcl_common.h"
#include "zcl/esp_zigbee_zcl_power_config.h"
#include "esp_log.h"
#include "ha/esp_zigbee_ha_standard.h"

static const char *TAG = "Zigbee";
static bool zigbee_network_joined = false;

void factory_reset()
{
    esp_zb_factory_reset();
}

bool is_network_joined(void)
{
    return zigbee_network_joined;
}

void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct)
{
    uint32_t *p_sg_p = signal_struct->p_app_signal;
    esp_err_t err_status = signal_struct->esp_err_status;
    esp_zb_app_signal_type_t sig_type = *p_sg_p;
    switch (sig_type)
    {
        case ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP:
            ESP_LOGI(TAG, "Initialize Zigbee stack");
            esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_INITIALIZATION);
            break;
        case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
        case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:
            ESP_LOGI(TAG, "Device started up in%s factory-reset mode", esp_zb_bdb_is_factory_new() ? "" : " non");
            if (esp_zb_bdb_is_factory_new())
            {
                ESP_LOGI(TAG, "Start network steering");
                esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
            }
            else
            {
                ESP_LOGI(TAG, "Device rebooted");
                zigbee_network_joined = esp_zb_get_short_address() != 0xFFFF && esp_zb_get_short_address() != 0xFFFE;
            }
            break;
        case ESP_ZB_BDB_SIGNAL_STEERING:
            esp_zb_ieee_addr_t extended_pan_id;
            esp_zb_get_extended_pan_id(extended_pan_id);
            if (err_status == ESP_OK)
            {
                ESP_LOGI(TAG, "Joined network successfully (Extended PAN ID: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x, PAN ID: 0x%04hx, Channel:%d, Short Address: 0x%04hx)",
                         extended_pan_id[7], extended_pan_id[6], extended_pan_id[5], extended_pan_id[4],
                         extended_pan_id[3], extended_pan_id[2], extended_pan_id[1], extended_pan_id[0],
                         esp_zb_get_pan_id(), esp_zb_get_current_channel(), esp_zb_get_short_address());

                zigbee_network_joined = true;
            }
            else
            {
                ESP_LOGW(TAG, "Неудачная попытка подключения к сети");
            }
            break;
        case ESP_ZB_ZDO_SIGNAL_LEAVE_INDICATION:
            zigbee_network_joined = false;
            ESP_LOGW(TAG, "Устройство отключено от сети");
            break;
        default:
            ESP_LOGI(TAG, "ZDO signal: %s (0x%x), status: %s", esp_zb_zdo_signal_to_string(sig_type), sig_type,
                    esp_err_to_name(err_status));
            break;
    }
}

static esp_zb_cluster_list_t *sensor_clusters_create()
{
    esp_zb_cluster_list_t *cluster_list = esp_zb_zcl_cluster_list_create();

    // Basic Cluster
    esp_zb_basic_cluster_cfg_t basic_config = {
        .zcl_version = ESP_ZB_ZCL_BASIC_ZCL_VERSION_DEFAULT_VALUE,
        .power_source = 0x03 // Battery
    };
    esp_zb_attribute_list_t *basic_cluster = esp_zb_basic_cluster_create(&basic_config);
    ESP_ERROR_CHECK(esp_zb_basic_cluster_add_attr(basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID, MANUFACTURER_NAME));
    ESP_ERROR_CHECK(esp_zb_basic_cluster_add_attr(basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID, MODEL_IDENTIFIER));
    ESP_ERROR_CHECK(esp_zb_cluster_list_add_basic_cluster(cluster_list, basic_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE));

    // Identify Cluster
    esp_zb_identify_cluster_cfg_t identity_config = {
        .identify_time = ESP_ZB_ZCL_IDENTIFY_IDENTIFY_TIME_DEFAULT_VALUE,
    };
    esp_zb_attribute_list_t *identity_cluster = esp_zb_identify_cluster_create(&identity_config);
    ESP_ERROR_CHECK(esp_zb_cluster_list_add_identify_cluster(cluster_list, identity_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE));
    ESP_ERROR_CHECK(esp_zb_cluster_list_add_identify_cluster(cluster_list, esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_IDENTIFY), ESP_ZB_ZCL_CLUSTER_CLIENT_ROLE));

    // Temperature Measurement Cluster
    esp_zb_temperature_meas_cluster_cfg_t temp_config = {
        .measured_value = 0,
        .min_value = -10000,
        .max_value = 10000,
    };
    esp_zb_attribute_list_t *temp_cluster = esp_zb_temperature_meas_cluster_create(&temp_config);
    ESP_ERROR_CHECK(esp_zb_cluster_list_add_temperature_meas_cluster(cluster_list, temp_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE));

    // Humidity Measurement Cluster
    esp_zb_humidity_meas_cluster_cfg_t humidity_config = {
        .measured_value = 0xFFFF,
        .min_value = 0,
        .max_value = 10000
    }; // 100% * 100
    esp_zb_attribute_list_t *humidity_cluster = esp_zb_humidity_meas_cluster_create(&humidity_config);
    ESP_ERROR_CHECK(esp_zb_cluster_list_add_humidity_meas_cluster(cluster_list, humidity_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE));

    // Pressure Measurement Cluster
    esp_zb_pressure_meas_cluster_cfg_t pressure_config = {
        .measured_value = 32768,
        .min_value = -32768,
        .max_value = 32767
    };
    esp_zb_attribute_list_t *pressure_cluster = esp_zb_pressure_meas_cluster_create(&pressure_config);
    ESP_ERROR_CHECK(esp_zb_cluster_list_add_pressure_meas_cluster(cluster_list, pressure_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE));
    
    // Power Configuration (Battery) Cluster
    esp_zb_power_config_cluster_cfg_t power_config = {
        .main_voltage = 30, // 3.0V
        .main_freq = 0,
        .main_voltage_max = 50,
        .main_alarm_mask = 0,
        .main_voltage_min = 0
    };
    // Создаем структуру атрибута
    esp_zb_attribute_list_t *power_cluster = esp_zb_power_config_cluster_create(&power_config);
    // Добавляем атрибут в кластер
    uint8_t battery_percent_remaining = 127;
    ESP_ERROR_CHECK(esp_zb_power_config_cluster_add_attr(power_cluster, ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_PERCENTAGE_REMAINING_ID, &battery_percent_remaining));
    ESP_ERROR_CHECK(esp_zb_cluster_list_add_power_config_cluster(cluster_list, power_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE));

    return cluster_list;
}

void zigbee_task(void *pvParameters) {
    esp_zb_platform_config_t config = {
        .radio_config = {
            .radio_mode = ZB_RADIO_MODE_NATIVE,
        },
        .host_config = {
            .host_connection_mode = ZB_HOST_CONNECTION_MODE_NONE
        }
    };
    ESP_ERROR_CHECK(esp_zb_platform_config(&config));

    esp_zb_cfg_t zb_cfg = {
        .esp_zb_role = ESP_ZB_DEVICE_TYPE_ED,
        .install_code_policy = INSTALLCODE_POLICY_ENABLE,
        .nwk_cfg.zed_cfg = {
            .ed_timeout = ED_AGING_TIMEOUT,
            .keep_alive = ED_KEEP_ALIVE,
        },
    };
    
    esp_zb_init(&zb_cfg);
    esp_zb_set_channel_mask(11); // Channel 11

    esp_zb_ep_list_t *ep_list = esp_zb_ep_list_create();
    esp_zb_endpoint_config_t endpoint_config = {
        .endpoint = HA_ESP_SENSOR_ENDPOINT,
        .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
        .app_device_id = ESP_ZB_HA_TEMPERATURE_SENSOR_DEVICE_ID,
        .app_device_version = 0};

    esp_zb_cluster_list_t *cluster_list = sensor_clusters_create(endpoint_config);

    ESP_ERROR_CHECK(esp_zb_ep_list_add_ep(ep_list, cluster_list, endpoint_config));

    ESP_ERROR_CHECK(esp_zb_device_register(ep_list));
    ESP_ERROR_CHECK(esp_zb_set_primary_network_channel_set(ESP_ZB_TRANSCEIVER_ALL_CHANNELS_MASK));
    ESP_ERROR_CHECK(esp_zb_start(true));
    esp_zb_stack_main_loop();
}

void update_attribute(uint16_t cluster_id, uint16_t attr_id, void *value_p)
{
    esp_zb_lock_acquire(portMAX_DELAY);
    esp_zb_zcl_set_attribute_val(HA_ESP_SENSOR_ENDPOINT,
                                 cluster_id,
                                 ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
                                 attr_id,
                                 value_p,
                                 false);

    esp_zb_zcl_report_attr_cmd_t report_attr_cmd = {
        .address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
        .attributeID = attr_id,
        .direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_CLI,
        .clusterID = cluster_id,
        .zcl_basic_cmd.src_endpoint = HA_ESP_SENSOR_ENDPOINT};

    esp_zb_zcl_report_attr_cmd_req(&report_attr_cmd);
    esp_zb_lock_release();
}

void update_temperature_value(int16_t temperature_degrees_tenths)
{
    update_attribute(ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT, ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_VALUE_ID, &temperature_degrees_tenths);
}

void update_humidity_value(uint16_t humidity_tenths)
{
    update_attribute(ESP_ZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT, ESP_ZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_VALUE_ID, &humidity_tenths);
}

void update_pressure_value(int16_t pressure_tenths)
{
    update_attribute(ESP_ZB_ZCL_CLUSTER_ID_PRESSURE_MEASUREMENT, ESP_ZB_ZCL_ATTR_PRESSURE_MEASUREMENT_VALUE_ID, &pressure_tenths);
}

void update_battery_value(uint8_t battery_percent)
{
    uint8_t value = battery_percent * 2;
    update_attribute(ESP_ZB_ZCL_CLUSTER_ID_POWER_CONFIG, ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_PERCENTAGE_REMAINING_ID, &value);
}