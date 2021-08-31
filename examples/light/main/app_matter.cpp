/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "esp_matter.h"
#include "esp_matter_standard.h"
#include "app_matter.h"
#include "app_constants.h"

#include "esp_heap_caps.h"
#include "esp_log.h"

#include "app/common/gen/att-storage.h"
#include "app/common/gen/attribute-id.h"
#include "app/common/gen/attribute-type.h"
#include "app/common/gen/cluster-id.h"
#include "app/server/Mdns.h"
#include "app/server/Server.h"
#include "app/util/af.h"
#include "app/util/basic-types.h"
#include "platform/CHIPDeviceLayer.h"
#include "core/CHIPError.h"
#include "lib/shell/Engine.h"
#include "lib/support/CHIPMem.h"
#include <cstdint>

using chip::AttributeId;
using chip::ClusterId;
using chip::EndpointId;
using chip::DeviceLayer::ChipDeviceEvent;
using chip::DeviceLayer::ConnectivityMgr;
using chip::DeviceLayer::DeviceEventType::PublicEventTypes;
using chip::DeviceLayer::PlatformMgr;
#if CHIP_DEVICE_CONFIG_ENABLE_THREAD
using chip::DeviceLayer::ThreadStackMgr;
#endif

typedef enum {
    REMAP_MATTER_TO_STANDARD,
    REMAP_STANDARD_TO_MATTER,
} app_matter_remap_t;

#define STANDARD_BRIGHTNESS 100
#define STANDARD_HUE 360
#define STANDARD_SATURATION 100
#define STANDARD_TEMPERATURE 100

#define MATTER_BRIGHTNESS 255
#define MATTER_HUE 255
#define MATTER_SATURATION 255
#define MATTER_TEMPERATURE 255

#define APP_MATTER_NAME "Matter"
static const char *TAG = "app_matter";

int app_matter_remap(char *attribute, int value, app_matter_remap_t remap)
{
    if (remap == REMAP_MATTER_TO_STANDARD) {
        if (strncmp(attribute, ESP_MATTER_ATTR_POWER, sizeof(ESP_MATTER_ATTR_POWER)) == 0) {
            return value;
        } else if (strncmp(attribute, ESP_MATTER_ATTR_BRIGHTNESS, sizeof(ESP_MATTER_ATTR_BRIGHTNESS)) == 0) {
            return REMAP_TO_RANGE(value, MATTER_BRIGHTNESS, STANDARD_BRIGHTNESS);
        } else if (strncmp(attribute, ESP_MATTER_ATTR_HUE, sizeof(ESP_MATTER_ATTR_HUE)) == 0) {
            return REMAP_TO_RANGE(value, MATTER_HUE, STANDARD_HUE);
        } else if (strncmp(attribute, ESP_MATTER_ATTR_SATURATION, sizeof(ESP_MATTER_ATTR_SATURATION)) == 0) {
            return REMAP_TO_RANGE(value, MATTER_SATURATION, STANDARD_SATURATION);
        } else if (strncmp(attribute, ESP_MATTER_ATTR_TEMPERATURE, sizeof(ESP_MATTER_ATTR_TEMPERATURE)) == 0) {
            return REMAP_TO_RANGE(value, MATTER_TEMPERATURE, STANDARD_TEMPERATURE);
        }
    } else if (remap == REMAP_STANDARD_TO_MATTER) {
        if (strncmp(attribute, ESP_MATTER_ATTR_POWER, sizeof(ESP_MATTER_ATTR_POWER)) == 0) {
            return value;
        } else if (strncmp(attribute, ESP_MATTER_ATTR_BRIGHTNESS, sizeof(ESP_MATTER_ATTR_BRIGHTNESS)) == 0) {
            return REMAP_TO_RANGE(value, STANDARD_BRIGHTNESS, MATTER_BRIGHTNESS);
        } else if (strncmp(attribute, ESP_MATTER_ATTR_HUE, sizeof(ESP_MATTER_ATTR_HUE)) == 0) {
            return REMAP_TO_RANGE(value, STANDARD_HUE, MATTER_HUE);
        } else if (strncmp(attribute, ESP_MATTER_ATTR_SATURATION, sizeof(ESP_MATTER_ATTR_SATURATION)) == 0) {
            return REMAP_TO_RANGE(value, STANDARD_SATURATION, MATTER_SATURATION);
        } else if (strncmp(attribute, ESP_MATTER_ATTR_TEMPERATURE, sizeof(ESP_MATTER_ATTR_TEMPERATURE)) == 0) {
            return REMAP_TO_RANGE(value, STANDARD_TEMPERATURE, MATTER_TEMPERATURE);
        }
    }
    return value;
}

static EndpointId app_matter_get_endpoint_id(const char *endpoint)
{
    if (strncmp(endpoint, ESP_MATTER_ENDPOINT_LIGHT, sizeof(ESP_MATTER_ENDPOINT_LIGHT)) == 0) {
        return 1;
    }
    return 0;
}

static const char *app_matter_get_endpoint_name(EndpointId endpoint)
{
    if (endpoint == 1) {
        return ESP_MATTER_ENDPOINT_LIGHT;
    }
    return NULL;
}

static ClusterId app_matter_get_cluster_id(const char *attribute)
{
    if (strncmp(attribute, ESP_MATTER_ATTR_POWER, sizeof(ESP_MATTER_ATTR_POWER)) == 0) {
        return ZCL_ON_OFF_CLUSTER_ID;
    } else if (strncmp(attribute, ESP_MATTER_ATTR_BRIGHTNESS, sizeof(ESP_MATTER_ATTR_BRIGHTNESS)) == 0) {
        return ZCL_LEVEL_CONTROL_CLUSTER_ID;
    } else if (strncmp(attribute, ESP_MATTER_ATTR_HUE, sizeof(ESP_MATTER_ATTR_HUE)) == 0) {
        return ZCL_COLOR_CONTROL_CLUSTER_ID;
    } else if (strncmp(attribute, ESP_MATTER_ATTR_SATURATION, sizeof(ESP_MATTER_ATTR_SATURATION)) == 0) {
        return ZCL_COLOR_CONTROL_CLUSTER_ID;
    } else if (strncmp(attribute, ESP_MATTER_ATTR_TEMPERATURE, sizeof(ESP_MATTER_ATTR_TEMPERATURE)) == 0) {
        return ZCL_COLOR_CONTROL_CLUSTER_ID;
    }
    return 0;
}

static AttributeId app_matter_get_attribute_id(const char *attribute)
{
    if (strncmp(attribute, ESP_MATTER_ATTR_POWER, sizeof(ESP_MATTER_ATTR_POWER)) == 0) {
        return ZCL_ON_OFF_ATTRIBUTE_ID;
    } else if (strncmp(attribute, ESP_MATTER_ATTR_BRIGHTNESS, sizeof(ESP_MATTER_ATTR_BRIGHTNESS)) == 0) {
        return ZCL_CURRENT_LEVEL_ATTRIBUTE_ID;
    } else if (strncmp(attribute, ESP_MATTER_ATTR_HUE, sizeof(ESP_MATTER_ATTR_HUE)) == 0) {
        return ZCL_COLOR_CONTROL_CURRENT_HUE_ATTRIBUTE_ID;
    } else if (strncmp(attribute, ESP_MATTER_ATTR_SATURATION, sizeof(ESP_MATTER_ATTR_SATURATION)) == 0) {
        return ZCL_COLOR_CONTROL_CURRENT_SATURATION_ATTRIBUTE_ID;
    } else if (strncmp(attribute, ESP_MATTER_ATTR_TEMPERATURE, sizeof(ESP_MATTER_ATTR_TEMPERATURE)) == 0) {
        return ZCL_COLOR_CONTROL_COLOR_TEMPERATURE_ATTRIBUTE_ID;
    }
    return 0;
}

static const char *app_matter_get_attribute_name(ClusterId cluster, AttributeId attribute)
{
    if (cluster == ZCL_ON_OFF_CLUSTER_ID) {
        return ESP_MATTER_ATTR_POWER;
    } else if (cluster == ZCL_LEVEL_CONTROL_CLUSTER_ID) {
        return ESP_MATTER_ATTR_BRIGHTNESS;
    } else if (cluster == ZCL_COLOR_CONTROL_CLUSTER_ID) {
        if (attribute == ZCL_COLOR_CONTROL_CURRENT_HUE_ATTRIBUTE_ID) {
            return ESP_MATTER_ATTR_HUE;
        } else if (attribute == ZCL_COLOR_CONTROL_CURRENT_SATURATION_ATTRIBUTE_ID) {
            return ESP_MATTER_ATTR_SATURATION;
        } else if (attribute == ZCL_COLOR_CONTROL_COLOR_TEMPERATURE_ATTRIBUTE_ID) {
            return ESP_MATTER_ATTR_TEMPERATURE;
        }
    }
    return NULL;
}

static EmberAfAttributeType app_matter_get_attribute_type(esp_matter_attr_val_t val)
{
    if (val.type == ESP_MATTER_VAL_TYPE_BOOLEAN) {
        return ZCL_BOOLEAN_ATTRIBUTE_TYPE;
    } else if (val.type == ESP_MATTER_VAL_TYPE_INTEGER) {
        return ZCL_INT8U_ATTRIBUTE_TYPE;
    }
    return 0;
}

static int app_matter_get_attribute_value(esp_matter_attr_val_t val)
{
    if (val.type == ESP_MATTER_VAL_TYPE_BOOLEAN) {
        return (int)val.val.b;
    } else if (val.type == ESP_MATTER_VAL_TYPE_INTEGER) {
        return val.val.i;
    }
    return 0;
}

static esp_matter_attr_val_t app_matter_get_attribute_val(char *attribute, int value)
{
    if (strncmp(attribute, ESP_MATTER_ATTR_POWER, sizeof(ESP_MATTER_ATTR_POWER)) == 0) {
        return esp_matter_bool((bool)value);
    } else if (strncmp(attribute, ESP_MATTER_ATTR_BRIGHTNESS, sizeof(ESP_MATTER_ATTR_BRIGHTNESS)) == 0) {
        return esp_matter_int(value);
    } else if (strncmp(attribute, ESP_MATTER_ATTR_HUE, sizeof(ESP_MATTER_ATTR_HUE)) == 0) {
        return esp_matter_int(value);
    } else if (strncmp(attribute, ESP_MATTER_ATTR_SATURATION, sizeof(ESP_MATTER_ATTR_SATURATION)) == 0) {
        return esp_matter_int(value);
    } else if (strncmp(attribute, ESP_MATTER_ATTR_TEMPERATURE, sizeof(ESP_MATTER_ATTR_TEMPERATURE)) == 0) {
        return esp_matter_int(value);
    }
    return esp_matter_int(value);
}

static esp_err_t app_matter_attribute_update(const char *endpoint, const char *attribute, esp_matter_attr_val_t val, void *priv_data)
{
    EndpointId endpoint_id = app_matter_get_endpoint_id(endpoint);
    ClusterId cluster_id = app_matter_get_cluster_id(attribute);
    AttributeId attribute_id = app_matter_get_attribute_id(attribute);
    EmberAfAttributeType attribute_type = app_matter_get_attribute_type(val);
    int value = app_matter_get_attribute_value(val);
    uint8_t value_remap = (uint8_t)app_matter_remap((char *)attribute, value, REMAP_STANDARD_TO_MATTER);
    ESP_LOGD(TAG, "Changing %s from standard: %d, to matter: %d\n", attribute, value, value_remap);

    EmberAfStatus status = emberAfWriteAttribute(endpoint_id, cluster_id, attribute_id, CLUSTER_MASK_SERVER, (uint8_t *)&value_remap, attribute_type);
    if (status != EMBER_ZCL_STATUS_SUCCESS) {
        ESP_LOGE(TAG, "Error updating attribute to matter");
        return ESP_FAIL;
    }
    return ESP_OK;
}

void emberAfPostAttributeChangeCallback(EndpointId endpoint, ClusterId cluster, AttributeId attribute, uint8_t mask, uint16_t manufacturer, uint8_t type, uint16_t size, uint8_t *value)
{
    char *endpoint_name = (char *)app_matter_get_endpoint_name(endpoint);
    char *attribute_name = (char *)app_matter_get_attribute_name(cluster, attribute);
    if (endpoint_name == NULL || attribute_name == NULL) {
        return;
    }
    int value_remap = app_matter_remap(attribute_name, (int)*value, REMAP_MATTER_TO_STANDARD);
    esp_matter_attr_val_t val = app_matter_get_attribute_val(attribute_name, value_remap);
    ESP_LOGD(TAG, "Changing %s from matter: %d, to standard: %d\n", attribute_name, *value, value_remap);

    esp_matter_attribute_notify(APP_MATTER_NAME, endpoint_name, attribute_name, val);
}

esp_err_t app_matter_attribute_set(const char *endpoint, const char *attribute, esp_matter_attr_val_t val)
{
    app_matter_attribute_update(endpoint, attribute, val, NULL);
    esp_matter_attribute_notify(APP_MATTER_NAME, endpoint, attribute, val);
    return ESP_OK;
}

static void on_device_event(const ChipDeviceEvent *event, intptr_t arg)
{
    if (event->Type == PublicEventTypes::kInterfaceIpAddressChanged) {
        chip::app::Mdns::StartServer();
    }
    ESP_LOGI(TAG, "Current free heap: %zu", heap_caps_get_free_size(MALLOC_CAP_8BIT));
}

esp_err_t app_matter_init()
{
    if (PlatformMgr().InitChipStack() != CHIP_NO_ERROR) {
        ESP_LOGE(TAG, "Failed to initialize CHIP stack");
        return ESP_FAIL;
    }
    ConnectivityMgr().SetBLEAdvertisingEnabled(true);
    if (chip::Platform::MemoryInit() != CHIP_NO_ERROR) {
        ESP_LOGE(TAG, "Failed to initialize CHIP memory pool");
        return ESP_ERR_NO_MEM;
    }
    if (PlatformMgr().StartEventLoopTask() != CHIP_NO_ERROR) {
        chip::Platform::MemoryShutdown();
        ESP_LOGE(TAG, "Failed to launch Matter main task");
        return ESP_FAIL;
    }
    PlatformMgr().AddEventHandler(on_device_event, static_cast<intptr_t>(NULL));
#if CHIP_DEVICE_CONFIG_ENABLE_THREAD
    if (ThreadStackMgr().InitThreadStack() != CHIP_NO_ERROR) {
        ESP_LOGE(APP_LOG_TAG, "Failed to initialize Thread stack");
        return ESP_FAIL;
    }

    if (ThreadStackMgr().StartThreadTask() != CHIP_NO_ERROR) {
        ESP_LOGE(APP_LOG_TAG, "Failed to launch Thread task");
        return ESP_FAIL;
    }
#endif
    InitServer();
#if CHIP_DEVICE_CONFIG_ENABLE_THREAD
    chip::app::Mdns::StartServer();
#endif

    esp_matter_attribute_callback_add(APP_MATTER_NAME, app_matter_attribute_update, NULL);
    return ESP_OK;
}
