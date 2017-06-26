/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2017 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
 *
 * Permission is hereby granted for use on ESPRESSIF SYSTEMS ESP8266 only, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "lwip/sockets.h"

#include "esp_wifi.h"
#include "esp_sta.h"
#include "esp_system.h"

#include "string.h"
#include "esp_alink.h"

#include "alink_export.h"
#include "alink_product.h"
#include "alink_json_parser.h"
#include "esp_info_store.h"
#include "esp_alink_log.h"

static const char *TAG = "esp_alink_main";

/**
 * @brief  Clear wifi information, restart the device into the config network mode
 */
alink_err_t alink_update_router()
{
    int ret = 0;
    ALINK_LOGI("clear wifi config");
    ret = esp_info_erase(NVS_KEY_WIFI_CONFIG);
    // ALINK_ERROR_CHECK(ret != 0, ALINK_ERR, "alink_erase");
    ALINK_LOGI("The system is about to be restarted");
    system_restart();
    return ALINK_OK;
}

static alink_err_t alink_connect_ap()
{
    alink_err_t ret = ALINK_ERR;
    struct station_config wifi_config;

    ret = esp_info_load(NVS_KEY_WIFI_CONFIG, &wifi_config, sizeof(struct station_config));

    if (ret > 0) {
        ret = platform_awss_connect_ap(WIFI_WAIT_TIME, wifi_config.ssid,
                                       wifi_config.password, 0, 0, wifi_config.bssid, 0);

        if (ret == ALINK_OK) {
            return ALINK_OK;
        }
    }

    alink_event_send(ALINK_EVENT_CONFIG_NETWORK);
    ALINK_LOGI("*********************************");
    ALINK_LOGI("*    ENTER SAMARTCONFIG MODE    *");
    ALINK_LOGI("*********************************");
    ret = awss_start();

    if (ret != ALINK_OK) {
        ALINK_LOGI("awss_start is err ret: %d", ret);
        system_restart();
    }

    return ALINK_OK;
}

/**
 * @brief Clear all the information of the device and return to the factory status
 */
typedef void (*TaskFunction_t)(void *);
alink_err_t alink_factory_setting()
{
    /* clear ota data  */
    ALINK_LOGI("*********************************");
    ALINK_LOGI("*          FACTORY RESET        *");
    ALINK_LOGI("*********************************");

    int ret = 0;
    ALINK_LOGI("clear all config");
    ret = esp_info_erase(ALINK_SPACE_NAME);
    ALINK_ERROR_CHECK(ret != 0, ALINK_ERR, "alink_erase");

    ALINK_LOGI("Switch to the previous version");
    ret = spi_flash_erase_sector(0x1FE000 / 4096);
    ALINK_ERROR_CHECK(ret != 0, ALINK_ERR, "alink_erase");

    ALINK_LOGI("reset user account binding");
    /*!< The alink_factory_reset function takes 1.5k of stack space if the timer stack overflows */
    xTaskCreate((TaskFunction_t)alink_factory_reset, "alink_factory_reset", 2048 / 4 , NULL, 9, NULL);
    vTaskDelay(1000 / portTICK_RATE_MS);
}

/**
 * @brief Get time from alink service
 */
#define TIME_STR_LEN    (32)
alink_err_t alink_get_time(unsigned int *utc_time)
{
    char buf[TIME_STR_LEN] = { 0 }, *attr_str;
    int size = TIME_STR_LEN, attr_len = 0;
    int ret;

    ret = alink_query("getAlinkTime", "{}", buf, &size);

    if (!ret) {
        attr_str = json_get_value_by_name(buf, size, "time", &attr_len, NULL);

        if (attr_str && utc_time) {
            sscanf(attr_str, "%u", utc_time);
        }
    }

    return ret;
}

/**
 * @brief Event management
 */
#define EVENT_QUEUE_NUM         3
static xQueueHandle xQueueEvent = NULL;
static void alink_event_loop_task(void *pvParameters)
{
    alink_err_t ret = ALINK_OK;
    alink_event_cb_t s_event_handler_cb = (alink_event_cb_t)pvParameters;

    for (;;) {
        alink_event_t event;

        if (xQueueReceive(xQueueEvent, &event, portMAX_DELAY) != pdPASS) {
            continue;
        }

        if (!s_event_handler_cb) {
            continue;
        }

        ret = (*s_event_handler_cb)(event);

        if (ret != ALINK_OK) {
            ALINK_LOGW("Event handling failed");
        }
    }

    vTaskDelete(NULL);
}

alink_err_t alink_event_send(alink_event_t event)
{
    if (!xQueueEvent) {
        xQueueEvent = xQueueCreate(EVENT_QUEUE_NUM, sizeof(alink_event_t));
    }

    alink_err_t ret = xQueueSend(xQueueEvent, &event, 0);
    ALINK_ERROR_CHECK(ret != pdTRUE, ALINK_ERR, "xQueueSendToBack fail!");
    return ALINK_OK;
}

/**
 * @brief Initialize alink config and start alink task
 */
extern alink_err_t alink_trans_init();
extern void alink_trans_destroy();
xTaskHandle event_handle = NULL;
alink_err_t alink_init(_IN_ const void *product_info,
                       _IN_ const alink_event_cb_t event_handler_cb)
{
    ALINK_PARAM_CHECK(!product_info);
    ALINK_PARAM_CHECK(!event_handler_cb);

    alink_err_t ret = ALINK_OK;

    if (!xQueueEvent) {
        xQueueEvent = xQueueCreate(EVENT_QUEUE_NUM, sizeof(alink_event_t));
    }

    xTaskCreate(alink_event_loop_task, "alink_event_loop_task", EVENT_HANDLER_CB_STACK,
                event_handler_cb, DEFAULU_TASK_PRIOTY, &event_handle);

    ret = product_set(product_info);
    ALINK_ERROR_CHECK(ret != ALINK_OK, ALINK_ERR, "product_set :%d", ret);

    ret = alink_connect_ap();
    ALINK_ERROR_CHECK(ret != ALINK_OK, ALINK_ERR, "alink_connect_ap :%d", ret);

    ret = alink_trans_init();

    if (ret != ALINK_OK) {
        alink_trans_destroy();
    }

    ALINK_ERROR_CHECK(ret != ALINK_OK, ALINK_ERR, "alink_trans_init :%d", ret);

    unsigned int alink_server_time = 0;
    ret = alink_get_time(&alink_server_time);
    ALINK_LOGD("ret: %d,get alink utc time: %d", ret, alink_server_time);

    return ALINK_OK;
}
