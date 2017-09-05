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

#include "esp_common.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_alink.h"
#include "esp_alink_log.h"
#include "esp_json_parser.h"

#ifndef ALINK_PASSTHROUGH

#ifndef CONFIG_READ_TASK_STACK
#define CONFIG_READ_TASK_STACK ((1024+512) / 4)
#endif

static const char *TAG = "sample_json";
static xTaskHandle read_handle = NULL;

/*do your job here*/
typedef struct virtual_dev {
    uint8_t errorcode;
    uint8_t hue;
    uint8_t luminance;
    uint8_t power;
    uint8_t work_mode;
} dev_info_t;

static dev_info_t light_info = {
    .errorcode = 0x00,
    .hue       = 0x10,
    .luminance = 0x50,
    .power     = 0x01,
    .work_mode = 0x02,
};

/**
 * @brief  In order to simplify the analysis of json package operations,
 * use the package esp_json_parse, you can also use the standard cJson data analysis
 */
static alink_err_t device_data_parse(const char *json_str, const char *key, uint8_t *value)
{
    alink_err_t ret = 0;
    char sub_str[64] = {0};
    char value_tmp[8] = {0};

    ret = esp_json_parse(json_str, key, sub_str);

    if (ret < 0) {
        return ALINK_ERR;
    }

    ret = esp_json_parse(sub_str, "value", value_tmp);

    if (ret < 0) {
        return ALINK_ERR;
    }

    *value = atoi(value_tmp);
    return ALINK_ERR;
}

static alink_err_t device_data_pack(const char *json_str, const char *key, int value)
{
    char sub_str[64] = {0};
    alink_err_t ret = 0;

    ret = esp_json_pack(sub_str, "value", value);

    if (ret < 0) {
        return ALINK_ERR;
    }

    ret = esp_json_pack(json_str, key, sub_str);

    if (ret < 0) {
        return ALINK_ERR;
    }

    return ALINK_OK;
}

/**
 * @brief When the service received errno a jump that is complete activation,
 *        activation of the order need to modify the specific equipment
 */
static const char *activate_data = "{\"ErrorCode\": { \"value\": \"1\" }}";
static const char *activate_data2 = "{\"ErrorCode\": { \"value\": \"0\" }}";
static alink_err_t alink_activate_device()
{
    alink_err_t ret = 0;
    ret = alink_write(activate_data, strlen(activate_data) + 1, 200);
    ret = alink_write(activate_data2, strlen(activate_data2) + 1, 200);

    if (ret < 0) {
        ALINK_LOGW("alink_write is err");
        return ALINK_ERR;
    }

    return ALINK_OK;
}

/**
 * @brief  The alink protocol specifies that the state of the device must be
 *         proactively attached to the Ali server
 */
static alink_err_t proactive_report_data()
{
    alink_err_t ret = 0;
    char *up_cmd = (char *)calloc(1, ALINK_DATA_LEN);

    // device_data_pack(up_cmd, "ErrorCode", light_info.errorcode);
    device_data_pack(up_cmd, "Hue", light_info.hue);
    device_data_pack(up_cmd, "Luminance", light_info.luminance);
    device_data_pack(up_cmd, "Switch", light_info.power);
    device_data_pack(up_cmd, "WorkMode", light_info.work_mode);
    ret = alink_write(up_cmd, strlen(up_cmd) + 1, 500);
    free(up_cmd);

    if (ret < 0) {
        ALINK_LOGW("alink_write is err");
        return ALINK_ERR;
    }

    return ALINK_OK;
}

/*
 * getDeviceStatus: {"attrSet":[],"uuid":"7DD5CE4ECE654B721BE8F4F912C10B8E"}
 * postDeviceData: {"Luminance":{"value":"80"},"Switch":{"value":"1"},"attrSet":["Luminance","Switch","Hue","ErrorCode","WorkMode","onlineState"],"Hue":{"value":"16"},"ErrorCode":{"value":"0"},"uuid":"158EE04889E2B1FE4BF18AFE4BFD0F04","WorkMode":{"value":"2"},"onlineState":{"when":"1495184488","value":"on"}
 *                 {"Switch":{"value":"1"},"attrSet":["Switch"],"uuid":"158EE04889E2B1FE4BF18AFE4BFD0F04"}
 *
 * @note  read_task_test stack space is small, need to follow the specific
 *        application to increase the size of the stack
 */
static void read_task_test(void *arg)
{
    char *down_cmd = (char *)malloc(ALINK_DATA_LEN);
    alink_err_t ret = ALINK_ERR;

    for (;;) {
        ret = alink_read(down_cmd, ALINK_DATA_LEN, portMAX_DELAY);

        if (ret < 0) {
            ALINK_LOGW("alink_read is err");
            continue;
        }

        char method_str[16] = {0};
        ret = esp_json_parse(down_cmd, "method", method_str);

        if (ret < 0) {
            ALINK_LOGW("esp_json_parse, ret: %d", ret);
            continue;
        }

        if (!strcmp(method_str, "getDeviceStatus")) {
            proactive_report_data();
            continue;
        }

        if (!strcmp(method_str, "setDeviceStatus")) {
            ALINK_LOGV("setDeviceStatus: %s", down_cmd);
            device_data_parse(down_cmd, "ErrorCode", &(light_info.errorcode));
            device_data_parse(down_cmd, "Hue", &(light_info.hue));
            device_data_parse(down_cmd, "Luminance", &(light_info.luminance));
            device_data_parse(down_cmd, "Switch", &(light_info.power));
            device_data_parse(down_cmd, "WorkMode", &(light_info.work_mode));
            ALINK_LOGI("read: errorcode:%d, hue: %d, luminance: %d, Switch: %d, work_mode: %d, free heap: %d",
                       light_info.errorcode, light_info.hue, light_info.luminance, light_info.power, light_info.work_mode,
                       system_get_free_heap_size());

            memset(down_cmd, 0, ALINK_DATA_LEN);
            device_data_pack(down_cmd, "Hue", light_info.hue);
            device_data_pack(down_cmd, "Luminance", light_info.luminance);
            device_data_pack(down_cmd, "Switch", light_info.power);
            device_data_pack(down_cmd, "WorkMode", light_info.work_mode);
            ret = alink_write(down_cmd, strlen(down_cmd) + 1, 0);

            if (ret < 0) {
                ALINK_LOGW("alink_write is err");
            }
        }
    }

    free(down_cmd);
    vTaskDelete(NULL);
}

static int count = 0; /*!< Count the number of packets received */
static alink_err_t alink_event_handler(alink_event_t event)
{
    switch (event) {
        case ALINK_EVENT_CLOUD_CONNECTED:
            ALINK_LOGD("Alink cloud connected!");
            proactive_report_data();
            break;

        case ALINK_EVENT_CLOUD_DISCONNECTED:
            ALINK_LOGD("Alink cloud disconnected!");
            break;

        case ALINK_EVENT_GET_DEVICE_DATA:
            ALINK_LOGD("The cloud initiates a query to the device");
            break;

        case ALINK_EVENT_SET_DEVICE_DATA:
            count++;
            ALINK_LOGD("The cloud is set to send instructions");
            break;

        case ALINK_EVENT_POST_CLOUD_DATA:
            ALINK_LOGD("The device post data success!");
            break;

        case ALINK_EVENT_WIFI_DISCONNECTED:
            ALINK_LOGD("Wifi disconnected");
            break;

        case ALINK_EVENT_CONFIG_NETWORK:
            ALINK_LOGD("Enter the network configuration mode");
            break;

        case ALINK_EVENT_UPDATE_ROUTER:
            ALINK_LOGD("Requests update router");
            alink_update_router();
            break;

        case ALINK_EVENT_FACTORY_RESET:
            ALINK_LOGD("Requests factory reset");
            alink_factory_setting();
            break;

        case ALINK_EVENT_ACTIVATE_DEVICE:
            ALINK_LOGD("Requests activate device");
            alink_activate_device();
            break;

        case ALINK_EVENT_HIGH_FREQUENCY_TEST:
            ALINK_LOGD("enter high-frequency send and receive data test");
            ALINK_LOGI("*********************************");
            ALINK_LOGI("*  SET ALINK SDK LOGLEVEL INFO  *");
            ALINK_LOGI("*********************************");
            /**
             * Too much serial print information will not be able to pass high-frequency
             * send and receive data test
             */
            alink_set_loglevel(ALINK_LL_INFO);
            break;

        default:
            break;
    }

    return ALINK_OK;
}

/******************************************************************************
 * FunctionName : user_rf_cal_sector_set
 * Description  : SDK just reversed 4 sectors, used for rf init data and paramters.
 *                We add this function to force users to set rf cal sector, since
 *                we don't know which sector is free in user's application.
 *                sector map for last several sectors : ABCCC
 *                A : rf cal
 *                B : rf init data
 *                C : sdk parameters
 * Parameters   : none
 * Returns      : rf cal sector
*******************************************************************************/
uint32 user_rf_cal_sector_set(void)
{
    flash_size_map size_map = system_get_flash_size_map();
    uint32 rf_cal_sec = 0;

    switch (size_map) {
        case FLASH_SIZE_4M_MAP_256_256:
            rf_cal_sec = 128 - 5;
            break;

        case FLASH_SIZE_8M_MAP_512_512:
            rf_cal_sec = 256 - 5;
            break;

        case FLASH_SIZE_16M_MAP_512_512:
        case FLASH_SIZE_16M_MAP_1024_1024:
            rf_cal_sec = 512 - 5;
            break;

        case FLASH_SIZE_32M_MAP_512_512:
        case FLASH_SIZE_32M_MAP_1024_1024:
            rf_cal_sec = 1024 - 5;
            break;

        case FLASH_SIZE_64M_MAP_1024_1024:
            rf_cal_sec = 2048 - 5;
            break;

        case FLASH_SIZE_128M_MAP_1024_1024:
            rf_cal_sec = 4096 - 5;
            break;

        default:
            rf_cal_sec = 0;
            break;
    }

    return rf_cal_sec;
}

/**
 * @brief As a unique identifier for the sds device
 */
#define device_key              "xrSJSzVDKPk4UB7BGCIf"
#define device_secret           "cRB3lwgd7zwFg02DK69xxl2lgefDdtYZ"
#define DEVICE_KEY_LEN          (20 + 1)
#define DEVICE_SECRET_LEN       (32 + 1)

char *product_get_device_key(char key_str[DEVICE_KEY_LEN])
{
    return strncpy(key_str, device_key, DEVICE_KEY_LEN);
}

char *product_get_device_secret(char secret_str[DEVICE_SECRET_LEN])
{
    return strncpy(secret_str, device_secret, DEVICE_SECRET_LEN);
}

static void alink_app_main_task(void *arg)
{
    const alink_product_t product_info = {
        .name           = "alink_product",
        /*!< Product version number, ota upgrade need to be modified */
        .version        = "1.0.0",
        .model          = "ALINKTEST_LIVING_LIGHT_ALINK_TEST",
        .key            = "5gPFl8G4GyFZ1fPWk20m",
        .secret         = "ngthgTlZ65bX5LpViKIWNsDPhOf2As9ChnoL9gQb",
        /*!< The Key-value pair used in the product */
        .key_sandbox    = "dpZZEpm9eBfqzK7yVeLq",
        .secret_sandbox = "THnfRRsU5vu6g6m9X6uFyAjUWflgZ0iyGjdEneKm",
    };

    /**
     * @brief You can use other trigger mode, to trigger the distribution network, activation and other operations
     */
    alink_key_trigger();
    esp_info_init();
    alink_init(&product_info, alink_event_handler);
    xTaskCreate(read_task_test, "read_task_test", CONFIG_READ_TASK_STACK, NULL, 5, &read_handle);
    vTaskDelete(NULL);
}

#ifdef SAMPLE_JSON_DEBUG
extern xTaskHandle event_handle;
extern xTaskHandle post_handle;
/**
 * @brief This function is only for detecting memory leaks
 */
static void alink_debug_task(void *arg)
{
    for (;;) {
        ALINK_LOGI("total free heap :%d, count: %d", system_get_free_heap_size(), count);

        if (event_handle) {
            ALINK_LOGI("event_handle free heap size: %dB", uxTaskGetStackHighWaterMark(event_handle) * 4);
        }

        if (post_handle) {
            ALINK_LOGI("post_handle free heap size: %dB", uxTaskGetStackHighWaterMark(post_handle) * 4);
        }

        if (read_handle) {
            ALINK_LOGI("read_handle free heap size: %dB", uxTaskGetStackHighWaterMark(read_handle) * 4);
        }

        printf("\n");
        vTaskDelay(5000 / portTICK_RATE_MS);
    }

    vTaskDelete(NULL);
}
#endif

/******************************************************************************
 * FunctionName : user_init
 * Description  : entry of user application, init user function here
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
void user_init(void)
{
#ifdef SAMPLE_JSON_DEBUG
    /*!< Used to debug unexpectedly exit */
    struct rst_info *rst_info = system_get_rst_info();
    ALINK_LOGI("reason   : 0x%x", rst_info->reason);
    ALINK_LOGI("exccause : 0x%x", rst_info->exccause);
    ALINK_LOGI("epc1     : 0x%x", rst_info->epc1);
    ALINK_LOGI("epc2     : 0x%x", rst_info->epc2);
    ALINK_LOGI("epc3     : 0x%x", rst_info->epc3);
    ALINK_LOGI("excvaddr : 0x%x", rst_info->excvaddr);
    ALINK_LOGI("depc     : 0x%x", rst_info->depc);
    ALINK_LOGI("rtn_addr : 0x%x", rst_info->rtn_addr);
    xTaskCreate(alink_debug_task, "debug_task", 512 / 4 , NULL, 3, NULL);
#endif

    ALINK_LOGI("================= SYSTEM INFO ================");
    ALINK_LOGI("compile time : %s %s", __DATE__, __TIME__);
    ALINK_LOGI("modle name   : %s", CONFIG_ALINK_MODULE_NAME);
    ALINK_LOGI("chip id      : %d", system_get_chip_id());
    ALINK_LOGI("sdk  version : %s", system_get_sdk_version());
    ALINK_LOGI("cup freq     : %dMHz", system_get_cpu_freq());
    ALINK_LOGI("free heap    : %dB", system_get_free_heap_size());

    wifi_set_opmode(STATION_MODE);
    xTaskCreate(alink_app_main_task, "app_main_task", (4096 + 512) / 4 , NULL, 5, NULL);
}
#endif
