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

#include <string.h>

#include "esp_system.h"
#include "upgrade.h"

#include "esp_alink.h"
#include "esp_alink_log.h"

static const char *TAG = "alink_upgrade";
static int binary_file_length = 0;

#define MAX_BIN_SIZE 700

void platform_flash_program_start(void)
{
    ALINK_LOGI("system upgrade init");
    system_upgrade_init();
    system_upgrade_flag_set(UPGRADE_FLAG_START);

    uint8_t buffer[1] = {0xff};
    alink_err_t ret = system_upgrade(buffer, MAX_BIN_SIZE * 1024);
    ALINK_ERROR_CHECK(ret != ALINK_TRUE, ; , "Error: esp_ota_write failed! err=0x%x", ret);
    ALINK_LOGI("system upgrade init succeeded");
}

int platform_flash_program_write_block(_IN_ char *buffer, _IN_ uint32_t length)
{
    ALINK_PARAM_CHECK(length == 0);
    ALINK_PARAM_CHECK(buffer == NULL);

    alink_err_t ret;
    ALINK_LOGV("buffer: %p, length: %d, free heap: %d", buffer, length, system_get_free_heap_size());
    ret = system_upgrade(buffer, length);
    ALINK_ERROR_CHECK(ret != ALINK_TRUE, ALINK_ERR, "Error: esp_ota_write failed! ret=0x%x", ret);

    binary_file_length += length;
    ALINK_LOGD("have written image length %d", binary_file_length);
    return ALINK_OK;
}

int platform_flash_program_stop(void)
{
    alink_err_t ret;
    ALINK_LOGI("total Write binary data length : %d", binary_file_length);

    if (binary_file_length > MAX_BIN_SIZE * 1024) {
        ALINK_LOGE("The actual size of bin is greater than the maximum value of the set bin. Please modify MAX_BIN_SIZE");
        return ALINK_ERR;
    }

    system_upgrade_flag_set(UPGRADE_FLAG_FINISH);

    ret = system_upgrade_flag_check();
    ALINK_ERROR_CHECK(ret != UPGRADE_FLAG_FINISH, ALINK_ERR, "system_upgrade_flag_check failed!");
    system_upgrade_deinit();
    system_upgrade_reboot();
    ALINK_LOGI("system upgrade finished, system restart");
    system_restart();
    return ALINK_OK;
}
