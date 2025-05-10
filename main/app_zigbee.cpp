#include <stdio.h>
#include "esp_zigbee_core.h"
#include "zcl/esp_zigbee_zcl_common.h"
#include "zb_config.h"
#include "zb_types.h"
#include "zcl/esp_zigbee_zcl_power_config.h"
#include "esp_log.h"
#include "ha/esp_zigbee_ha_standard.h"
#include "nvs_flash.h"
#include "app_zigbee.h"

static const char *TAG = "Zigbee";

static zigbee_network_event_cb reboot_success_callback = []() {};
static zigbee_network_event_cb factory_reset_mode_callback = []() {};
static zigbee_network_event_cb network_joined_callback = []() {};
static zigbee_network_event_cb connection_failed_callback = []() {};

static int8_t retry_count = 1;

void factory_reset()
{
    esp_zb_factory_reset();
}

void register_reboot_success_callback(zigbee_network_event_cb cb)
{
    reboot_success_callback = cb;
}

void register_factory_reset_mode_callback(zigbee_network_event_cb cb)
{
    factory_reset_mode_callback = cb;
}

void register_network_joined_callback(zigbee_network_event_cb cb)
{
    network_joined_callback = cb;
}

void register_connection_failed_callback(zigbee_network_event_cb cb)
{
    connection_failed_callback = cb;
}

static void bdb_start_top_level_commissioning_cb(uint8_t mode_mask)
{
    esp_zb_bdb_start_top_level_commissioning(mode_mask);
}

void send_device_announce(void)
{
    esp_zb_lock_acquire(portMAX_DELAY);

    esp_zb_zdo_device_announcement_req();

    esp_zb_lock_release();
}

void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct)
{
    uint32_t *p_sg_p = signal_struct->p_app_signal;
    esp_err_t err_status = signal_struct->esp_err_status;
    esp_zb_app_signal_type_t sig_type = (esp_zb_app_signal_type_t)(*p_sg_p);
    switch (sig_type)
    {
        case ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP:
            ESP_LOGI(TAG, "Initialize Zigbee stack");
            esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_INITIALIZATION);
            break;
        case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
        case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:
            if (err_status == ESP_OK)
            {
                if (esp_zb_bdb_is_factory_new())
                {
                    factory_reset_mode_callback();
                    ESP_LOGI(TAG, "Start network steering");
                    esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
                }
                else
                {
                    ESP_LOGI(TAG, "Device rebooted");
                    reboot_success_callback();
                }
            }
            else
            {
                if (retry_count-- > 0)
                {
                    ESP_LOGW(TAG, "%s failed with status: %s. Retrying %d", esp_zb_zdo_signal_to_string(sig_type), esp_err_to_name(err_status), retry_count);
                    esp_zb_scheduler_alarm((esp_zb_callback_t)bdb_start_top_level_commissioning_cb, ESP_ZB_BDB_MODE_INITIALIZATION, 1000);
                }
                else
                {
                    connection_failed_callback();
                }
            }
            break;
        case ESP_ZB_BDB_SIGNAL_STEERING:
            if (err_status == ESP_OK)
            {
                esp_zb_ieee_addr_t extended_pan_id;
                esp_zb_get_extended_pan_id(extended_pan_id);
                ESP_LOGI(TAG, "Joined network successfully (Extended PAN ID: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x, PAN ID: 0x%04hx, Channel:%d, Short Address: 0x%04hx)",
                        extended_pan_id[7], extended_pan_id[6], extended_pan_id[5], extended_pan_id[4],
                        extended_pan_id[3], extended_pan_id[2], extended_pan_id[1], extended_pan_id[0],
                        esp_zb_get_pan_id(), esp_zb_get_current_channel(), esp_zb_get_short_address());

                network_joined_callback();
            }
            else
            {
                ESP_LOGW(TAG, "Network steering was not successful (status: %s). Retrying...", esp_err_to_name(err_status));
                esp_zb_scheduler_alarm((esp_zb_callback_t)bdb_start_top_level_commissioning_cb, ESP_ZB_BDB_MODE_NETWORK_STEERING, 1000);
            }
            break;    
        case ESP_ZB_COMMON_SIGNAL_CAN_SLEEP:
            break;
        case ESP_ZB_ZDO_SIGNAL_LEAVE_INDICATION:
            ESP_LOGW(TAG, "Устройство отключено от сети");
            break;
        default:
            ESP_LOGI(TAG, "ZDO signal: %s (0x%x), status: %s", esp_zb_zdo_signal_to_string(sig_type), sig_type, esp_err_to_name(err_status));
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
    ESP_ERROR_CHECK(esp_zb_basic_cluster_add_attr(basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID, (void *)MANUFACTURER_NAME));
    ESP_ERROR_CHECK(esp_zb_basic_cluster_add_attr(basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID, (void *)MODEL_IDENTIFIER));
    ESP_ERROR_CHECK(esp_zb_cluster_list_add_basic_cluster(cluster_list, basic_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE));

    // Identify Cluster
    esp_zb_identify_cluster_cfg_t identity_config = {
        .identify_time = ESP_ZB_ZCL_IDENTIFY_IDENTIFY_TIME_DEFAULT_VALUE,
    };
    esp_zb_attribute_list_t *identity_cluster = esp_zb_identify_cluster_create(&identity_config);
    ESP_ERROR_CHECK(esp_zb_cluster_list_add_identify_cluster(cluster_list, identity_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE));

    // ------------------------------- Температура -------------------------------
    int16_t temp_measured_value = 0; // 0.00°C (в сотых долях градуса Цельсия)
    int16_t temp_min_value = -10000; // -100.00°C
    int16_t temp_max_value = 10000;  // 100.00°C
    uint16_t temp_tolerance = 10;     // 0.1°C (допуск в сотых долях градуса Цельсия)

    esp_zb_attribute_list_t *temp_attr_list = esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT);
    ESP_ERROR_CHECK(esp_zb_cluster_add_attr(temp_attr_list, ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT, ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_VALUE_ID, ESP_ZB_ZCL_ATTR_TYPE_S16, ESP_ZB_ZCL_ATTR_ACCESS_REPORTING, &temp_measured_value));
    ESP_ERROR_CHECK(esp_zb_cluster_add_attr(temp_attr_list, ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT, ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_MIN_VALUE_ID, ESP_ZB_ZCL_ATTR_TYPE_S16, ESP_ZB_ZCL_ATTR_ACCESS_REPORTING, &temp_min_value));
    ESP_ERROR_CHECK(esp_zb_cluster_add_attr(temp_attr_list, ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT, ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_MAX_VALUE_ID, ESP_ZB_ZCL_ATTR_TYPE_S16, ESP_ZB_ZCL_ATTR_ACCESS_REPORTING, &temp_max_value));
    ESP_ERROR_CHECK(esp_zb_cluster_add_attr(temp_attr_list, ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT, ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_TOLERANCE_ID, ESP_ZB_ZCL_ATTR_TYPE_U16, ESP_ZB_ZCL_ATTR_ACCESS_REPORTING, &temp_tolerance));
    ESP_ERROR_CHECK(esp_zb_cluster_list_add_temperature_meas_cluster(cluster_list, temp_attr_list, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE));

    // ------------------------------- Влажность -------------------------------
    int16_t hum_measured_value = 0xFFFF;
    int16_t hum_min_value = 0;
    int16_t hum_max_value = 10000;
    uint16_t hum_tolerance = 10;

    esp_zb_attribute_list_t *humidity_attr_list = esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT);
    ESP_ERROR_CHECK(esp_zb_cluster_add_attr(humidity_attr_list, ESP_ZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT, ESP_ZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_VALUE_ID, ESP_ZB_ZCL_ATTR_TYPE_U16, ESP_ZB_ZCL_ATTR_ACCESS_REPORTING, &hum_measured_value));
    ESP_ERROR_CHECK(esp_zb_cluster_add_attr(humidity_attr_list, ESP_ZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT, ESP_ZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_MIN_VALUE_ID, ESP_ZB_ZCL_ATTR_TYPE_S16, ESP_ZB_ZCL_ATTR_ACCESS_REPORTING, &hum_min_value));
    ESP_ERROR_CHECK(esp_zb_cluster_add_attr(humidity_attr_list, ESP_ZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT, ESP_ZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_MAX_VALUE_ID, ESP_ZB_ZCL_ATTR_TYPE_S16, ESP_ZB_ZCL_ATTR_ACCESS_REPORTING, &hum_max_value));
    ESP_ERROR_CHECK(esp_zb_cluster_add_attr(humidity_attr_list, ESP_ZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT, ESP_ZB_ZCL_ATTR_REL_HUMIDITY_TOLERANCE_ID, ESP_ZB_ZCL_ATTR_TYPE_U16, ESP_ZB_ZCL_ATTR_ACCESS_REPORTING, &hum_tolerance));
    ESP_ERROR_CHECK(esp_zb_cluster_list_add_humidity_meas_cluster(cluster_list, humidity_attr_list, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE));

    // ------------------------------- Давление -------------------------------
    int16_t pres_measured_value = 32768;
    int16_t pres_min_value = -32768;
    int16_t pres_max_value = 32767;
    uint16_t pres_tolerance = 1;

    esp_zb_attribute_list_t *pressure_attr_list = esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_PRESSURE_MEASUREMENT);
    ESP_ERROR_CHECK(esp_zb_cluster_add_attr(pressure_attr_list, ESP_ZB_ZCL_CLUSTER_ID_PRESSURE_MEASUREMENT, ESP_ZB_ZCL_ATTR_PRESSURE_MEASUREMENT_VALUE_ID, ESP_ZB_ZCL_ATTR_TYPE_S16, ESP_ZB_ZCL_ATTR_ACCESS_REPORTING, &pres_measured_value));
    ESP_ERROR_CHECK(esp_zb_cluster_add_attr(pressure_attr_list, ESP_ZB_ZCL_CLUSTER_ID_PRESSURE_MEASUREMENT, ESP_ZB_ZCL_ATTR_PRESSURE_MEASUREMENT_MIN_VALUE_ID, ESP_ZB_ZCL_ATTR_TYPE_S16, ESP_ZB_ZCL_ATTR_ACCESS_REPORTING, &pres_min_value));
    ESP_ERROR_CHECK(esp_zb_cluster_add_attr(pressure_attr_list, ESP_ZB_ZCL_CLUSTER_ID_PRESSURE_MEASUREMENT, ESP_ZB_ZCL_ATTR_PRESSURE_MEASUREMENT_MAX_VALUE_ID, ESP_ZB_ZCL_ATTR_TYPE_S16, ESP_ZB_ZCL_ATTR_ACCESS_REPORTING, &pres_max_value));
    ESP_ERROR_CHECK(esp_zb_cluster_add_attr(pressure_attr_list, ESP_ZB_ZCL_CLUSTER_ID_PRESSURE_MEASUREMENT, ESP_ZB_ZCL_ATTR_PRESSURE_MEASUREMENT_TOLERANCE_ID, ESP_ZB_ZCL_ATTR_TYPE_U16, ESP_ZB_ZCL_ATTR_ACCESS_REPORTING, &pres_tolerance));
    ESP_ERROR_CHECK(esp_zb_cluster_list_add_pressure_meas_cluster(cluster_list, pressure_attr_list, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE));

    // ------------------------------- Питание -------------------------------
    uint8_t battery_percent_value = 100;

    esp_zb_attribute_list_t *power_attr_list = esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_POWER_CONFIG);
    ESP_ERROR_CHECK(esp_zb_cluster_add_attr(power_attr_list, ESP_ZB_ZCL_CLUSTER_ID_POWER_CONFIG, ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_PERCENTAGE_REMAINING_ID, ESP_ZB_ZCL_ATTR_TYPE_U8, ESP_ZB_ZCL_ATTR_ACCESS_REPORTING, &battery_percent_value));
    ESP_ERROR_CHECK(esp_zb_cluster_list_add_power_config_cluster(cluster_list, power_attr_list, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE));

    return cluster_list;
}

void zigbee_task(void *pvParameters) {
    // esp_zb_sleep_enable(true);

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_LOGW(TAG, "Erasing NVS...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

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
        .nwk_cfg = {
            .zed_cfg = {
                .ed_timeout = ED_AGING_TIMEOUT,
                .keep_alive = ED_KEEP_ALIVE,
            }
        },
    };

    esp_zb_init(&zb_cfg);

    esp_zb_ep_list_t *ep_list = esp_zb_ep_list_create();
    esp_zb_endpoint_config_t endpoint_config = {
        .endpoint = HA_ESP_SENSOR_ENDPOINT,
        .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
        .app_device_id = ESP_ZB_HA_TEMPERATURE_SENSOR_DEVICE_ID,
        .app_device_version = 0};

    esp_zb_cluster_list_t *cluster_list = sensor_clusters_create();

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
        .zcl_basic_cmd = {
            .src_endpoint = (zb_uint8_t)HA_ESP_SENSOR_ENDPOINT
        },
        .address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
        .clusterID = cluster_id,
        .manuf_specific = {0},
        .direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_CLI,
        .dis_defalut_resp = {0},
        .manuf_code = {0},
        .attributeID = attr_id
    };

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