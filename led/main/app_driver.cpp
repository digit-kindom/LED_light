/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <esp_log.h>
#include <stdlib.h>
#include <string.h>

#include <device.h>
#include <esp_matter.h>
#include <led_driver.h>

#include <app_priv.h>

#include <color_format.h>
#include <driver/rmt.h>
#include <esp_log.h>
#include <led_strip.h>
#include <led_driver.h>

static uint8_t current_brightness = 0;
static uint32_t current_temp = 0;
static HS_color_t current_HS = {0, 0};
static RGB_color_t mRGB;

using namespace chip::app::Clusters;
using namespace esp_matter;

static const char *TAG = "app_driver";
extern uint16_t light_endpoint_id;
//extern uint16_t LED_fx_id;

void TaskWS2812Ranbow(void *p);

void TaskWS2812OneByOne(void *p);

/* Do any conversions/remapping for the actual value here */
static esp_err_t app_driver_light_set_power(led_driver_handle_t handle, esp_matter_attr_val_t *val) {
    return led_driver_set_power(handle, val->val.b);
}

static esp_err_t app_driver_light_set_brightness(led_driver_handle_t handle, esp_matter_attr_val_t *val) {
    int value = REMAP_TO_RANGE(val->val.u8, MATTER_BRIGHTNESS, STANDARD_BRIGHTNESS);
    return led_driver_set_brightness(handle, value);
}

static esp_err_t app_driver_light_set_hue(led_driver_handle_t handle, esp_matter_attr_val_t *val) {
    int value = REMAP_TO_RANGE(val->val.u8, MATTER_HUE, STANDARD_HUE);
    return led_driver_set_hue(handle, value);
}

static esp_err_t app_driver_light_set_saturation(led_driver_handle_t handle, esp_matter_attr_val_t *val) {
    int value = REMAP_TO_RANGE(val->val.u8, MATTER_SATURATION, STANDARD_SATURATION);
    return led_driver_set_saturation(handle, value);
}

static esp_err_t app_driver_light_set_temperature(led_driver_handle_t handle, esp_matter_attr_val_t *val) {
    uint32_t value = REMAP_TO_RANGE_INVERSE(val->val.u16, STANDARD_TEMPERATURE_FACTOR);
    return led_driver_set_temperature(handle, value);
}

static void app_driver_button_toggle_cb(void *arg, void *data) {
    ESP_LOGI(TAG, "Toggle button pressed");
    uint16_t endpoint_id = light_endpoint_id;
    uint32_t cluster_id = OnOff::Id;
    uint32_t attribute_id = OnOff::Attributes::OnOff::Id;

    node_t *node = node::get();
    endpoint_t *endpoint = endpoint::get(node, endpoint_id);
    cluster_t *cluster = cluster::get(endpoint, cluster_id);
    attribute_t *attribute = attribute::get(cluster, attribute_id);

    esp_matter_attr_val_t val = esp_matter_invalid(NULL);
    attribute::get_val(attribute, &val);
    val.val.b = !val.val.b;
    attribute::update(endpoint_id, cluster_id, attribute_id, &val);
}

// 定义任务句柄
TaskHandle_t xTaskHandle = nullptr;

esp_err_t app_driver_attribute_update(app_driver_handle_t driver_handle, uint16_t endpoint_id, uint32_t cluster_id,
                                      uint32_t attribute_id, esp_matter_attr_val_t *val) {
    esp_err_t err = ESP_OK;
    ESP_LOGI(
        TAG,
        "Enter cb 9999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999")
    ;
    ESP_LOGI(TAG, "cluster_id = %d ; attribute_id = %d ; val = %d", (int)cluster_id, (int)attribute_id, val->val.i);
    if (endpoint_id == light_endpoint_id) {
        led_driver_handle_t handle = (led_driver_handle_t) driver_handle;
        if (cluster_id == OnOff::Id) {
            if (attribute_id == OnOff::Attributes::OnOff::Id) {
                err = app_driver_light_set_power(handle, val);
            }
        } else if (cluster_id == LevelControl::Id) {
            if (attribute_id == LevelControl::Attributes::CurrentLevel::Id) {
                err = app_driver_light_set_brightness(handle, val);
            }
        } else if (cluster_id == ColorControl::Id) {
            if (attribute_id == ColorControl::Attributes::CurrentHue::Id) {
                err = app_driver_light_set_hue(handle, val);
            } else if (attribute_id == ColorControl::Attributes::CurrentSaturation::Id) {
                err = app_driver_light_set_saturation(handle, val);
            } else if (attribute_id == ColorControl::Attributes::ColorTemperatureMireds::Id) {
                err = app_driver_light_set_temperature(handle, val);
            }
        }
        if (cluster_id == ModeSelect::Id) {
            if (attribute_id == ModeSelect::Attributes::CurrentMode::Id) {
                switch (val->val.i) {
                    case 0:
                        // 关闭所有灯光特效
                        if (xTaskHandle != nullptr) {
                            vTaskDelete(xTaskHandle); // 终止任务
                            xTaskHandle = nullptr; // 清空句柄
                            app_driver_light_set_defaults(light_endpoint_id);
                        }
                        break;
                    case 1:
                        // 创建彩虹灯光任务
                        if (xTaskHandle == nullptr) {
                            xTaskCreate(TaskWS2812Ranbow, "ranbow", 2048, handle, 5, &xTaskHandle);
                        } else {
                            vTaskDelete(xTaskHandle); // 终止任务
                            xTaskHandle = nullptr; // 清空句柄
                            xTaskCreate(TaskWS2812Ranbow, "ranbow", 2048, handle, 5, &xTaskHandle);
                        }
                        break;
                    case 2:
                        // 灯效3
                        // 创建彩虹灯光任务
                        if (xTaskHandle == nullptr) {
                            //led_driver_set_power(handle, false);
                            xTaskCreate(TaskWS2812OneByOne, "onebyone", 4096, handle, 5, &xTaskHandle);
                        } else {
                            vTaskDelete(xTaskHandle); // 终止任务
                            xTaskHandle = nullptr; // 清空句柄
                            //led_driver_set_power(handle, false);
                            xTaskCreate(TaskWS2812OneByOne, "onebyone", 4096, handle, 5, &xTaskHandle);
                        }
                        break;
                    default:
                        break;
                }
            }
        }
    }
    return err;
}

void TaskWS2812Ranbow(void *p) {
    led_strip_t *strip = (led_strip_t *) p;
    u_int64_t start_rgb = 0;
    current_HS.saturation = 100;
    while (true) {
        for (int i = 0; i < 1; i++) {
            for (int j = i; j < 144; j += 1) {
                // Build RGB values
                current_HS.hue = j * 360 / 24 + start_rgb;
                hsv_to_rgb(current_HS, 100, &mRGB);
                // Write RGB values to strip driver
                strip->set_pixel(strip, j, mRGB.red, mRGB.green, mRGB.blue);
            } // Flush RGB values to LEDs
        }
        strip->refresh(strip, 100);
        vTaskDelay(pdMS_TO_TICKS(3));
        start_rgb -= 1;
    }
}

void TaskWS2812OneByOne(void *p) {
    led_strip_t *strip = (led_strip_t *) p;
    u_int64_t start_rgb = 0;
    current_HS.saturation = 100;
    //uint8_t nums = 5;
    while (true) {
        led_driver_set_power(strip, false);
        for (int j = 0; j < 144; j++) {
            // Build RGB values
            current_HS.hue = j * 360 / 24 + start_rgb;
            hsv_to_rgb(current_HS, 100, &mRGB);
            // Write RGB values to strip driver
            strip->set_pixel(strip, j, mRGB.red, mRGB.green, mRGB.blue);
            strip->refresh(strip, 100);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        for (int j = 0; j < 144; j++) {
            strip->set_pixel(strip, j, 0, 0, 0);
            strip->refresh(strip, 100);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        for (int j = 143; j >= 0; j--) {
            // Build RGB values
            current_HS.hue = j * 360 / 24 + start_rgb;
            hsv_to_rgb(current_HS, 100, &mRGB);
            // Write RGB values to strip driver
            strip->set_pixel(strip, j, mRGB.red, mRGB.green, mRGB.blue);
            strip->refresh(strip, 100);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        for (int j = 143; j >= 0; j--) {
            strip->set_pixel(strip, j, 0, 0, 0);
            strip->refresh(strip, 100);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        start_rgb +=50;
    }
}



esp_err_t app_driver_light_set_defaults(uint16_t endpoint_id) {
    esp_err_t err = ESP_OK;
    void *priv_data = endpoint::get_priv_data(endpoint_id);
    led_driver_handle_t handle = (led_driver_handle_t) priv_data;
    node_t *node = node::get();
    endpoint_t *endpoint = endpoint::get(node, endpoint_id);
    cluster_t *cluster = NULL;
    attribute_t *attribute = NULL;
    esp_matter_attr_val_t val = esp_matter_invalid(NULL);

    /* Setting brightness */
    cluster = cluster::get(endpoint, LevelControl::Id);
    attribute = attribute::get(cluster, LevelControl::Attributes::CurrentLevel::Id);
    attribute::get_val(attribute, &val);
    err |= app_driver_light_set_brightness(handle, &val);


    /* Setting color */
    cluster = cluster::get(endpoint, ColorControl::Id);
    attribute = attribute::get(cluster, ColorControl::Attributes::ColorMode::Id);
    attribute::get_val(attribute, &val);
    if (val.val.u8 == (uint8_t) ColorControl::ColorMode::kCurrentHueAndCurrentSaturation) {
        /* Setting hue */
        attribute = attribute::get(cluster, ColorControl::Attributes::CurrentHue::Id);
        attribute::get_val(attribute, &val);
        err |= app_driver_light_set_hue(handle, &val);
        /* Setting saturation */
        attribute = attribute::get(cluster, ColorControl::Attributes::CurrentSaturation::Id);
        attribute::get_val(attribute, &val);
        err |= app_driver_light_set_saturation(handle, &val);
    } else if (val.val.u8 == (uint8_t) ColorControl::ColorMode::kColorTemperature) {
        /* Setting temperature */
        attribute = attribute::get(cluster, ColorControl::Attributes::ColorTemperatureMireds::Id);
        attribute::get_val(attribute, &val);
        err |= app_driver_light_set_temperature(handle, &val);
    } else {
        ESP_LOGE(TAG, "Color mode not supported");
    }

    /* Setting power */
    cluster = cluster::get(endpoint, OnOff::Id);
    attribute = attribute::get(cluster, OnOff::Attributes::OnOff::Id);
    attribute::get_val(attribute, &val);
    err |= app_driver_light_set_power(handle, &val);
    return err;
}

app_driver_handle_t app_driver_light_init() {
    /* Initialize led */
    led_driver_config_t config = led_driver_get_config();
    led_driver_handle_t handle = led_driver_init(&config);
    return (app_driver_handle_t) handle;
}

app_driver_handle_t app_driver_button_init() {
    /* Initialize button */
    button_config_t config = button_driver_get_config();
    button_handle_t handle = iot_button_create(&config);
    iot_button_register_cb(handle, BUTTON_PRESS_DOWN, app_driver_button_toggle_cb, NULL);
    return (app_driver_handle_t) handle;
}
