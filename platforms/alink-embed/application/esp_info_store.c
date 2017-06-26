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

#include "c_types.h"

#include "esp_libc.h"

#include "esp_info_store.h"
#include "spi_flash.h"
#include "esp_alink_log.h"

static const char *TAG = "esp_info_store";

typedef struct alink_info {
    char key[INFO_STORE_KEY_LEN];
    uint16_t offset;
    uint16_t length;
} alink_info_t;

typedef struct alink_info_manager {
    uint8_t flag;
    uint8_t num;
    uint16_t use_length;
    alink_info_t alink_info[5];
} alink_info_manager_t;

static alink_info_manager_t g_info_manager;
static int get_alink_info_index(const char *key)
{
    int i = 0;

    for (i = 0; i < g_info_manager.num; ++i) {
        if (!strncmp(g_info_manager.alink_info[i].key, key, INFO_STORE_KEY_LEN)) {
            return i;
        }
    }

    return ALINK_ERR;
}

alink_err_t esp_info_init()
{
    int res = 0;
    uint32_t  value = 0;
    res = spi_flash_read(INFO_STORE_MANAGER_ADDR, (uint32_t *)&g_info_manager, sizeof(alink_info_manager_t));

    if (res != SPI_FLASH_RESULT_OK) {
        os_printf("read flash data error\n");
        return ALINK_ERR;
    }

    if (g_info_manager.num == 0xff || g_info_manager.num == 0x0) {
        spi_flash_erase_sector(INFO_STORE_MANAGER_ADDR / 4096);
        g_info_manager.num = 0;
        g_info_manager.use_length = sizeof(alink_info_manager_t);
    } else {
        int i = 0;

        for (i = 0; i < g_info_manager.num; ++i) {
            ALINK_LOGD("index: %d   key: %s", i, g_info_manager.alink_info[i].key);
        }
    }

    ALINK_LOGD("alink info addr: 0x%x, num: %d", INFO_STORE_MANAGER_ADDR, g_info_manager.num);
    return ALINK_OK;
}

int esp_info_erase(const char *key)
{
    ALINK_PARAM_CHECK(!key);

    if (!strcmp(key, ALINK_SPACE_NAME)) {
        spi_flash_erase_sector(INFO_STORE_MANAGER_ADDR / 4096);
        return ALINK_OK;
    }

    int info_index = get_alink_info_index(key);
    ALINK_ERROR_CHECK(info_index < 0, ALINK_ERR, "get_alink_info_index");

    uint8_t *data_tmp = malloc(g_info_manager.use_length);
    int ret = spi_flash_read(INFO_STORE_MANAGER_ADDR, (uint32_t *)data_tmp, g_info_manager.use_length);
    memset(data_tmp + g_info_manager.alink_info[info_index].offset, 0xff,
           g_info_manager.alink_info[info_index].length);
    spi_flash_erase_sector(INFO_STORE_MANAGER_ADDR / 4096);
    spi_flash_write(INFO_STORE_MANAGER_ADDR, (uint32_t *)data_tmp, g_info_manager.use_length);
    free(data_tmp);

    return ALINK_OK;
}

ssize_t esp_info_save(const char *key, const void *value, size_t length)
{
    ALINK_PARAM_CHECK(!key);
    ALINK_PARAM_CHECK(!value);
    ALINK_PARAM_CHECK(length <= 0);

    if (length & 0x03 != 0) {
        length += (4 - length & 0x3);
    }

    int i = 0;
    int ret = 0;
    int info_index = get_alink_info_index(key);

    if (info_index < 0) {
        alink_info_t *alink_info = &g_info_manager.alink_info[g_info_manager.num];
        strncpy(alink_info->key, key, INFO_STORE_KEY_LEN);
        alink_info->length = length;
        alink_info->offset = g_info_manager.use_length;
        g_info_manager.use_length += length;
        info_index = g_info_manager.num;
        g_info_manager.num++;
    }

    uint8_t *data_tmp = malloc(g_info_manager.use_length);
    ret = spi_flash_read(INFO_STORE_MANAGER_ADDR, (uint32_t *)data_tmp, g_info_manager.use_length);

    memcpy(data_tmp, &g_info_manager, sizeof(alink_info_manager_t));
    memcpy(data_tmp + g_info_manager.alink_info[info_index].offset, value,
           g_info_manager.alink_info[info_index].length);
    spi_flash_erase_sector(INFO_STORE_MANAGER_ADDR / 4096);
    ret = spi_flash_write(INFO_STORE_MANAGER_ADDR, (uint32_t *)data_tmp, g_info_manager.use_length);
    free(data_tmp);

    if (ret != SPI_FLASH_RESULT_OK) {
        return ALINK_ERR;
    }

    return ALINK_OK;
}

ssize_t esp_info_load(const char *key, void *value, size_t length)
{
    ALINK_PARAM_CHECK(!key);
    ALINK_PARAM_CHECK(!value);
    ALINK_PARAM_CHECK(length <= 0);

    if (length & 0x03 != 0) {
        length += (4 - length & 0x3);
    }

    int ret = 0;
    int info_index = get_alink_info_index(key);

    if (info_index < 0) {
        ALINK_LOGW("The data has been erased");
        return ALINK_ERR;
    }

    ALINK_ERROR_CHECK(length < g_info_manager.alink_info[info_index].length, ALINK_ERR,
                      "The buffer is too small ");
    ret = spi_flash_read(INFO_STORE_MANAGER_ADDR + g_info_manager.alink_info[info_index].offset,
                         (uint32_t *)value, length);

    if (*((uint8_t *)value) == 0xff && *((uint8_t *)value + 1) == 0xff) {
        ALINK_LOGW("The data has been erased");
        return ALINK_ERR;
    }

    return length;
}
