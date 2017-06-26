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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "lwip/sockets.h"

#include "esp_system.h"

#include "alink_export.h"
#include "esp_alink.h"
#include "esp_alink_log.h"
#include "esp_json_parser.h"

#define Method_PostData     "postDeviceData"
#define Method_PostRawData  "postDeviceRawData"
#define Method_GetStatus    "getDeviceStatus"
#define Method_SetStatus    "setDeviceStatus"
#define Method_GetAlinkTime "getAlinkTime"

#ifdef ALINK_PASSTHROUGH
#define ALINK_METHOD_POST     Method_PostRawData
#define ALINK_GET_DEVICE_DATA ALINK_GET_DEVICE_RAWDATA
#define ALINK_SET_DEVICE_DATA ALINK_SET_DEVICE_RAWDATA
#else /*!< ALINK_PASSTHROUGH */
#define ALINK_METHOD_POST     Method_PostData
#define ALINK_GET_DEVICE_DATA ALINK_GET_DEVICE_STATUS
#define ALINK_SET_DEVICE_DATA ALINK_SET_DEVICE_STATUS
#endif /*!< ALINK_PASSTHROUGH */

static const char *TAG = "esp_alink_trans";
static alink_err_t post_data_enable     = ALINK_TRUE;
static xQueueHandle xQueueDownCmd       = NULL;
static xQueueHandle xQueueUpCmd         = NULL;

#define alink_free(arg) {free(arg); \
        ALINK_LOGV("free ptr: %p, heap free: %d", arg, esp_get_free_heap_size()); arg = NULL;}while(0)
#define alink_malloc(num_bytes) ({void *ptr = malloc(num_bytes); \
        ALINK_LOGV("malloc size: %d, ptr: %p, heap free: %d", num_bytes, ptr, esp_get_free_heap_size()); ptr;})

static alink_err_t cloud_get_device_data(_IN_ char *json_buffer)
{
    ALINK_PARAM_CHECK(!json_buffer);
    alink_event_send(ALINK_EVENT_GET_DEVICE_DATA);
    alink_err_t ret = ALINK_OK;

    ret = esp_json_pack(json_buffer, "method", (char *)Method_GetStatus);
    ALINK_ERROR_CHECK(ret < 0, ALINK_ERR, "esp_json_pack, ret:%d", ret);

    int size = ret + 1;
    char *q_data = (char *)alink_malloc(size);

    if (size > ALINK_DATA_LEN) {
        ALINK_LOGW("json_buffer len:%d", size);
    }

    memcpy(q_data, json_buffer, size);

    if (xQueueSend(xQueueDownCmd, &q_data, 0) != pdTRUE) {
        ALINK_LOGW("xQueueSend xQueueDownCmd is err");
        ret = ALINK_ERR;
        alink_free(q_data);
    }

    return ret;
}

static alink_err_t cloud_set_device_data(_IN_ char *json_buffer)
{
    ALINK_PARAM_CHECK(!json_buffer);
    alink_event_send(ALINK_EVENT_SET_DEVICE_DATA);
    alink_err_t ret = ALINK_OK;

    ret = esp_json_pack(json_buffer, "method", (char *)Method_SetStatus);
    ALINK_ERROR_CHECK(ret < 0, ALINK_ERR, "esp_json_pack, ret:%d", ret);

    int size = ret + 1;
    char *q_data = (char *)alink_malloc(size);

    if (size > ALINK_DATA_LEN) {
        ALINK_LOGW("json_buffer len:%d", size);
    }

    memcpy(q_data, json_buffer, size);

    if (xQueueSend(xQueueDownCmd, &q_data, 0) != pdTRUE) {
        ALINK_LOGW("xQueueSend xQueueDownCmd is err");
        ret = ALINK_ERR;
        alink_free(q_data);
    }

    return ret;
}

static void alink_post_data(void *arg)
{
    alink_err_t ret;
    char *up_cmd = NULL;

    for (; post_data_enable;) {
        ret = xQueueReceive(xQueueUpCmd, &up_cmd, portMAX_DELAY);

        if (ret != pdTRUE) {
            ALINK_LOGW("There is no data to report");
            continue;
        }

        ALINK_LOGV("up_cmd: %s", up_cmd);
        ret = alink_report(ALINK_METHOD_POST, up_cmd);

        if (ret != ALINK_OK) {
            ALINK_LOGW("post failed!");
            platform_msleep(1000);
        } else {
            alink_event_send(ALINK_EVENT_POST_CLOUD_DATA);
        }

        alink_free(up_cmd);
    }

    vTaskDelete(NULL);
}

static void cloud_connected(void)
{
    alink_event_send(ALINK_EVENT_CLOUD_CONNECTED);
}

static void cloud_disconnected(void)
{
    alink_event_send(ALINK_EVENT_CLOUD_DISCONNECTED);
}

xTaskHandle post_handle = NULL;
alink_err_t alink_trans_init()
{
    alink_err_t ret  = ALINK_OK;
    post_data_enable = ALINK_TRUE;
    xQueueUpCmd      = xQueueCreate(DOWN_CMD_QUEUE_NUM, sizeof(char *));
    xQueueDownCmd    = xQueueCreate(UP_CMD_QUEUE_NUM, sizeof(char *));
    alink_set_loglevel(ALINK_SDK_LOG_LEVEL);

    alink_register_callback(ALINK_CLOUD_CONNECTED, &cloud_connected);
    alink_register_callback(ALINK_CLOUD_DISCONNECTED, &cloud_disconnected);
    alink_register_callback(ALINK_GET_DEVICE_DATA, &cloud_get_device_data);
    alink_register_callback(ALINK_SET_DEVICE_DATA, &cloud_set_device_data);

    ret = alink_start();
    ALINK_ERROR_CHECK(ret != ALINK_OK, ALINK_ERR, "alink_start :%d", ret);
    ALINK_LOGI("wait main device login");
    /*wait main device login, -1 means wait forever */
    ret = alink_wait_connect(ALINK_WAIT_FOREVER);
    ALINK_ERROR_CHECK(ret != ALINK_OK, ALINK_ERR, "alink_start : %d", ret);
    ret = xTaskCreate(alink_post_data, "alink_post_data", ALINK_POST_DATA_STACK, NULL, DEFAULU_TASK_PRIOTY, &post_handle);
    ALINK_ERROR_CHECK(ret != pdTRUE, ALINK_ERR, "thread_create name: %s, stack_size: %d, ret: %d", "alink_post_data", 2048, ret);
    return ALINK_OK;
}

void alink_trans_destroy()
{
    post_data_enable = ALINK_FALSE;
    alink_end();
    vQueueDelete(xQueueUpCmd);
    vQueueDelete(xQueueDownCmd);
}

#ifdef ALINK_PASSTHROUGH
#define RawDataHeader   "{\"rawData\":\""
#define RawDataTail     "\", \"length\":\"%d\"}"
ssize_t alink_write(_IN_ const void *up_cmd, size_t len, int micro_seconds)
{
    ALINK_PARAM_CHECK(up_cmd == NULL);
    ALINK_PARAM_CHECK(len == 0 || len > ALINK_DATA_LEN);

    if (!xQueueUpCmd) {
        return ALINK_ERR;
    }

    int i = 0;
    alink_err_t ret = ALINK_OK;
    char *q_data = (char *)alink_malloc(ALINK_DATA_LEN);

    int size = strlen(RawDataHeader);
    strncpy((char *)q_data, RawDataHeader, ALINK_DATA_LEN);

    for (i = 0; i < len; i++) {
        size += snprintf((char *)q_data + size,
                         ALINK_DATA_LEN - size, "%02X", ((uint8_t *)up_cmd)[i]);
    }

    size += snprintf((char *)q_data + size,
                     ALINK_DATA_LEN - size, RawDataTail, len * 2);

    ret = xQueueSend(xQueueUpCmd, &q_data, micro_seconds / portTICK_RATE_MS);

    if (ret == pdFALSE) {
        ALINK_LOGW("xQueueSend xQueueUpCmd, wait_time: %d", micro_seconds);
        alink_free(q_data);
    } else {
        ret = size;
    }

    return ret;
}

static alink_err_t raw_data_unserialize(char *json_buffer, uint8_t *raw_data, int *raw_data_len)
{
    int attr_len = 0, i = 0;
    char *attr_str = NULL;

    attr_str = json_get_value_by_name(json_buffer, strlen(json_buffer),
                                      "rawData", &attr_len, NULL);

    if (!attr_str || !attr_len || attr_len > *raw_data_len * 2) {
        return ALINK_ERR;
    }

    int raw_data_tmp = 0;

    for (i = 0; i < attr_len; i += 2) {
        sscanf(&attr_str[i], "%02x", &raw_data_tmp);
        raw_data[i / 2] = raw_data_tmp;
    }

    *raw_data_len = attr_len / 2;

    return ALINK_OK;
}

ssize_t alink_read(_OUT_ void *down_cmd, size_t size, int micro_seconds)
{
    ALINK_PARAM_CHECK(down_cmd == NULL);
    ALINK_PARAM_CHECK(size == 0 || size > ALINK_DATA_LEN);

    if (!xQueueDownCmd) {
        return ALINK_ERR;
    }

    alink_err_t ret = ALINK_OK;
    char *q_data = NULL;
    ret = xQueueReceive(xQueueDownCmd, &q_data, micro_seconds / portTICK_RATE_MS);

    if (ret == pdFALSE) {
        ALINK_LOGE("xQueueReceive xQueueDownCmd, ret:%d, wait_time: %d", ret, micro_seconds);
        ret = ALINK_ERR;
        goto EXIT;
    }

    if (strlen(q_data) + 1 > ALINK_DATA_LEN) {
        ALINK_LOGW("read len > ALINK_DATA_LEN, len: %d", strlen(q_data) + 1);
        ret = ALINK_DATA_LEN;
        goto EXIT;
    }

    ret = raw_data_unserialize(q_data, (uint8_t *)down_cmd, (int *)&size);

    if (ret != ALINK_OK) {
        ALINK_LOGW("raw_data_unserialize, ret:%d", ret);
    } else {
        ret = size;
    }

EXIT:
    alink_free(q_data);
    return ret;
}

#else /*!< ALINK_PASSTHROUGH */

ssize_t alink_write(_IN_ const void *up_cmd, size_t size, int micro_seconds)
{
    ALINK_PARAM_CHECK(up_cmd == NULL);
    ALINK_PARAM_CHECK(size == 0 || size > ALINK_DATA_LEN);

    if (!xQueueUpCmd) {
        return ALINK_ERR;
    }

    alink_err_t ret = ALINK_OK;
    char *q_data = (char *)alink_malloc(size);
    memcpy(q_data, up_cmd, size);
    ret = xQueueSend(xQueueUpCmd, &q_data, micro_seconds / portTICK_RATE_MS);

    if (ret == pdFALSE) {
        ALINK_LOGD("xQueueSend xQueueUpCmd, wait_time: %d", micro_seconds);
        alink_free(q_data);
        return ALINK_ERR;
    }

    return size;
}

ssize_t alink_read(_OUT_ void *down_cmd, size_t size, int micro_seconds)
{
    ALINK_PARAM_CHECK(down_cmd == NULL);
    ALINK_PARAM_CHECK(size == 0 || size > ALINK_DATA_LEN);

    if (!xQueueDownCmd) {
        return ALINK_ERR;
    }

    alink_err_t ret = ALINK_OK;

    char *q_data = NULL;
    ret = xQueueReceive(xQueueDownCmd, &q_data, micro_seconds / portTICK_RATE_MS);

    if (ret == pdFALSE) {
        ALINK_LOGD("xQueueReceive xQueueDownCmd, ret:%d, wait_time: %d", ret, micro_seconds);
        alink_free(q_data);
        return ALINK_ERR;
    }

    char size_tmp = strlen(q_data) + 1;
    size = (size_tmp > size) ? size : size_tmp;

    if (size > ALINK_DATA_LEN) {
        ALINK_LOGD("read len > ALINK_DATA_LEN, len: %d", size);
        size = ALINK_DATA_LEN;
        q_data[size - 1] = '\0';
    }

    memcpy(down_cmd, q_data, size);

    alink_free(q_data);
    return size;
}
#endif /*!< ALINK_PASSTHROUGH */
